// Copyright (c) 2020-2021 Sultim Tsyrendashiev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "TextureManager.h"

#include "Const.h"
#include "RgException.h"
#include "TextureExporter.h"
#include "TextureOverrides.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

#include <numeric>

using namespace RTGL1;


namespace
{

constexpr MaterialTextures EmptyMaterialTextures = { EMPTY_TEXTURE_INDEX,
                                                     EMPTY_TEXTURE_INDEX,
                                                     EMPTY_TEXTURE_INDEX };
static_assert( TEXTURES_PER_MATERIAL_COUNT == 3 );

constexpr RgSamplerFilter DefaultDynamicSamplerFilter = RG_SAMPLER_FILTER_LINEAR;

constexpr bool PreferExistingMaterials = true;

template< typename T >
constexpr const T* DefaultIfNull( const T* pData, const T* pDefault )
{
    return pData != nullptr ? pData : pDefault;
}

TextureOverrides::Loader GetLoader( const std::shared_ptr< ImageLoader >&    defaultLoader,
                                    const std::shared_ptr< ImageLoaderDev >& devLoader )
{
    return devLoader ? TextureOverrides::Loader( devLoader.get() )
                     : TextureOverrides::Loader( defaultLoader.get() );
}

bool ContainsTextures( const MaterialTextures& m )
{
    for( uint32_t t : m.indices )
    {
        if( t != EMPTY_TEXTURE_INDEX )
        {
            return true;
        }
    }
    return false;
}

auto FindEmptySlot( std::vector< Texture >& textures )
{
    return std::ranges::find_if( textures, []( const Texture& t ) {
        return t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE;
    } );
}

}


TextureManager::TextureManager( VkDevice                                _device,
                                std::shared_ptr< MemoryAllocator >      _memAllocator,
                                std::shared_ptr< SamplerManager >       _samplerMgr,
                                std::shared_ptr< CommandBufferManager > _cmdManager,
                                const std::filesystem::path &           _waterNormalTexturePath,
                                RgTextureSwizzling                      _pbrSwizzling,
                                bool                                    _forceNormalMapFilterLinear,
                                const LibraryConfig::Config&            _config )
    : device( _device )
    , pbrSwizzling( _pbrSwizzling )
    , memAllocator( std::move( _memAllocator ) )
    , cmdManager( std::move( _cmdManager ) )
    , samplerMgr( std::move( _samplerMgr ) )
    , waterNormalTextureIndex( 0 )
    , currentDynamicSamplerFilter( DefaultDynamicSamplerFilter )
    , postfixes
        {
            DEFAULT_TEXTURE_POSTFIX_ALBEDO_ALPHA,
            DEFAULT_TEXTURE_POSTFIX_ROUGNESS_METALLIC_EMISSION,
            DEFAULT_TEXTURE_POSTFIX_NORMAL,
        }
    , forceNormalMapFilterLinear(_forceNormalMapFilterLinear  )
{
    imageLoader = std::make_shared< ImageLoader >();

    if( _config.developerMode )
    {
        imageLoaderDev = std::make_shared< ImageLoaderDev >( imageLoader );
    }


    textureDesc = std::make_shared< TextureDescriptors >(
        device, samplerMgr, TEXTURE_COUNT_MAX, BINDING_TEXTURES );
    textureUploader = std::make_shared< TextureUploader >( device, memAllocator );

    textures.resize( TEXTURE_COUNT_MAX );

    // submit cmd to create empty texture
    {
        VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();
        {
            CreateEmptyTexture( cmd, 0 );
            CreateWaterNormalTexture( cmd, 0, _waterNormalTexturePath );
        }
        cmdManager->Submit( cmd );
        cmdManager->WaitGraphicsIdle();
    }
}

void TextureManager::CreateEmptyTexture( VkCommandBuffer cmd, uint32_t frameIndex )
{
    assert( textures[ EMPTY_TEXTURE_INDEX ].image == VK_NULL_HANDLE &&
            textures[ EMPTY_TEXTURE_INDEX ].view == VK_NULL_HANDLE );

    constexpr uint32_t   data[] = { 0xFFFFFFFF };
    constexpr RgExtent2D size   = { 1, 1 };

    ImageLoader::ResultInfo info = {
        .levelSizes     = { sizeof( data ) },
        .levelCount     = 1,
        .isPregenerated = false,
        .pData          = reinterpret_cast< const uint8_t* >( data ),
        .dataSize       = sizeof( data ),
        .baseSize       = size,
        .format         = VK_FORMAT_R8G8B8A8_UNORM,
    };

    uint32_t textureIndex =
        PrepareTexture( cmd,
                        frameIndex,
                        info,
                        SamplerManager::Handle( RG_SAMPLER_FILTER_NEAREST,
                                                RG_SAMPLER_ADDRESS_MODE_REPEAT,
                                                RG_SAMPLER_ADDRESS_MODE_REPEAT ),
                        false,
                        "Empty texture",
                        false,
                        std::nullopt,
                        {},
                        FindEmptySlot( textures ) );

    // must have specific index
    assert( textureIndex == EMPTY_TEXTURE_INDEX );

    VkImage     emptyImage = textures[ textureIndex ].image;
    VkImageView emptyView  = textures[ textureIndex ].view;

    assert( emptyImage != VK_NULL_HANDLE && emptyView != VK_NULL_HANDLE );

    // if texture will be reset, it will use empty texture's info
    textureDesc->SetEmptyTextureInfo( emptyView );
}

// Check CreateStaticMaterial for notes
void TextureManager::CreateWaterNormalTexture( VkCommandBuffer              cmd,
                                               uint32_t                     frameIndex,
                                               const std::filesystem::path& filepath )
{
    if( !std::filesystem::exists( filepath ) )
    {
        this->waterNormalTextureIndex = EMPTY_TEXTURE_INDEX;
        debug::Warning( "Water normal texture fail: Can't find file: {}", filepath.string() );
        return;
    }

    constexpr uint32_t   defaultData[] = { 0x7F7FFFFF };
    constexpr RgExtent2D defaultSize   = { 1, 1 };

    // try to load image file
    TextureOverrides ovrd(
        filepath, "", "", defaultData, defaultSize, VK_FORMAT_R8G8B8A8_UNORM, imageLoader.get() );

    this->waterNormalTextureIndex =
        PrepareTexture( cmd,
                        frameIndex,
                        ovrd.result,
                        SamplerManager::Handle( RG_SAMPLER_FILTER_LINEAR,
                                                RG_SAMPLER_ADDRESS_MODE_REPEAT,
                                                RG_SAMPLER_ADDRESS_MODE_REPEAT ),
                        true,
                        "Water normal",
                        false,
                        std::nullopt,
                        std::move( ovrd.path ),
                        FindEmptySlot( textures ) );

    if( this->waterNormalTextureIndex == EMPTY_TEXTURE_INDEX )
    {
        debug::Warning( "Water normal texture fail: Couldn't upload texture. Path: {}",
                        filepath.string() );
    }
}

TextureManager::~TextureManager()
{
    for( auto& texture : textures )
    {
        assert( ( texture.image == VK_NULL_HANDLE && texture.view == VK_NULL_HANDLE ) ||
                ( texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE ) );

        if( texture.image != VK_NULL_HANDLE )
        {
            DestroyTexture( texture );
        }
    }

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        for( auto& texture : texturesToDestroy[ i ] )
        {
            DestroyTexture( texture );
        }
    }
}

void TextureManager::PrepareForFrame( uint32_t frameIndex )
{
    // destroy delayed textures
    for( auto& t : texturesToDestroy[ frameIndex ] )
    {
        DestroyTexture( t );
    }
    texturesToDestroy[ frameIndex ].clear();

    // clear staging buffer that are not in use
    textureUploader->ClearStaging( frameIndex );
}

void TextureManager::TryHotReload( VkCommandBuffer cmd, uint32_t frameIndex )
{
    for( auto& filepathNoExt : texturesToReloadNoExt )
    {
        for( auto slot = textures.begin(); slot < textures.end(); ++slot )
        {
            if( slot->image == VK_NULL_HANDLE || slot->view == VK_NULL_HANDLE )
            {
                continue;
            }

            constexpr bool isUpdateable = false;

            bool sameWithoutExt =
                std::filesystem::path( slot->filepath ).replace_extension( "" ) == filepathNoExt;

            if( sameWithoutExt )
            {
                TextureOverrides ovrd( filepathNoExt,
                                       "",
                                       "",
                                       nullptr,
                                       {},
                                       slot->format,
                                       GetLoader( imageLoader, imageLoaderDev ) );

                if( ovrd.result )
                {
                    const auto prevSampler   = slot->samplerHandle;
                    const auto prevSwizzling = slot->swizzling;

                    AddToBeDestroyed( frameIndex, *slot );

                    auto tindex = PrepareTexture( cmd,
                                                  frameIndex,
                                                  ovrd.result,
                                                  prevSampler,
                                                  true,
                                                  ovrd.debugname,
                                                  isUpdateable,
                                                  prevSwizzling,
                                                  std::move( ovrd.path ),
                                                  slot );

                    // must match, so materials' indices are still correct
                    assert( tindex == std::distance( textures.begin(), slot ) );

                    break;
                }
            }
        }
    }
    texturesToReloadNoExt.clear();
}

void TextureManager::SubmitDescriptors( uint32_t                         frameIndex,
                                        const RgDrawFrameTexturesParams* pTexturesParams,
                                        bool                             forceUpdateAllDescriptors )
{
    // check if dynamic sampler filter was changed
    RgSamplerFilter newDynamicSamplerFilter = pTexturesParams != nullptr
                                                  ? pTexturesParams->dynamicSamplerFilter
                                                  : DefaultDynamicSamplerFilter;

    if( currentDynamicSamplerFilter != newDynamicSamplerFilter )
    {
        currentDynamicSamplerFilter = newDynamicSamplerFilter;
        forceUpdateAllDescriptors   = true;
    }


    if( forceUpdateAllDescriptors )
    {
        textureDesc->ResetAllCache( frameIndex );
    }

    // update desc set with current values
    for( uint32_t i = 0; i < textures.size(); i++ )
    {
        textures[ i ].samplerHandle.SetIfHasDynamicSamplerFilter( newDynamicSamplerFilter );


        if( textures[ i ].image != VK_NULL_HANDLE )
        {
            textureDesc->UpdateTextureDesc(
                frameIndex, i, textures[ i ].view, textures[ i ].samplerHandle );
        }
        else
        {
            // reset descriptor to empty texture
            textureDesc->ResetTextureDesc( frameIndex, i );
        }
    }

    textureDesc->FlushDescWrites();
}

bool TextureManager::TryCreateMaterial( VkCommandBuffer              cmd,
                                        uint32_t                     frameIndex,
                                        const RgOriginalTextureInfo& info,
                                        const std::filesystem::path& folder )
{
    if( Utils::IsCstrEmpty( info.pTextureName ) )
    {
        debug::Warning( "RgOriginalTextureInfo::pTextureName must not be null or an empty string" );
        return false;
    }

    if( info.pPixels == nullptr )
    {
        debug::Warning( "RgOriginalTextureInfo::pPixels must not be null" );
        return false;
    }

    if( PreferExistingMaterials )
    {
        if( materials.contains( info.pTextureName ) )
        {
            debug::Warning( "{}: Material with the same name already exists. "
                            "Ignoring the new one.",
                            info.pTextureName );
            return false;
        }
    }


    // clang-format off
    TextureOverrides ovrd[] = {
        TextureOverrides( folder, info.pTextureName, postfixes[ 0 ], info.pPixels, info.size, VK_FORMAT_R8G8B8A8_SRGB, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( folder, info.pTextureName, postfixes[ 1 ], nullptr, {}, VK_FORMAT_R8G8B8A8_UNORM, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( folder, info.pTextureName, postfixes[ 2 ], nullptr, {}, VK_FORMAT_R8G8B8A8_UNORM, GetLoader( imageLoader, imageLoaderDev ) ),
    };
    static_assert( std::size( ovrd ) == TEXTURES_PER_MATERIAL_COUNT );
    // clang-format on


    SamplerManager::Handle samplers[] = {
        SamplerManager::Handle( info.filter, info.addressModeU, info.addressModeV ),
        SamplerManager::Handle( info.filter, info.addressModeU, info.addressModeV ),
        SamplerManager::Handle( forceNormalMapFilterLinear ? RG_SAMPLER_FILTER_LINEAR : info.filter,
                                info.addressModeU,
                                info.addressModeV ),
    };
    static_assert( std::size( samplers ) == TEXTURES_PER_MATERIAL_COUNT );
    static_assert( MATERIAL_NORMAL_INDEX == 2 );


    std::optional< RgTextureSwizzling > swizzlings[] = {
        std::nullopt,
        std::optional( pbrSwizzling ),
        std::nullopt,
    };
    static_assert( std::size( swizzlings ) == TEXTURES_PER_MATERIAL_COUNT );
    static_assert( MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX == 1 );


    MakeMaterial( cmd, frameIndex, info.pTextureName, ovrd, samplers, swizzlings );
    return true;
}

void TextureManager::MakeMaterial( VkCommandBuffer                                  cmd,
                                   uint32_t                                         frameIndex,
                                   std::string_view                                 materialName,
                                   std::span< TextureOverrides >                    ovrd,
                                   std::span< SamplerManager::Handle >              samplers,
                                   std::span< std::optional< RgTextureSwizzling > > swizzlings )
{
    assert( ovrd.size() == TEXTURES_PER_MATERIAL_COUNT );
    assert( samplers.size() == TEXTURES_PER_MATERIAL_COUNT );
    assert( swizzlings.size() == TEXTURES_PER_MATERIAL_COUNT );

    constexpr bool isUpdateable = false;

    MaterialTextures mtextures = {};
    for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
    {
        mtextures.indices[ i ] = PrepareTexture( cmd,
                                                 frameIndex,
                                                 ovrd[ i ].result,
                                                 samplers[ i ],
                                                 true,
                                                 ovrd[ i ].debugname,
                                                 isUpdateable,
                                                 swizzlings[ i ],
                                                 std::move( ovrd[ i ].path ),
                                                 FindEmptySlot( textures ) );
    }

    InsertMaterial( frameIndex,
                    materialName,
                    Material{
                        .textures     = mtextures,
                        .isUpdateable = isUpdateable,
                    } );
}

bool TextureManager::TryCreateMaterial( VkCommandBuffer                     cmd,
                                        uint32_t                            frameIndex,
                                        const std::string&                  materialName,
                                        std::span< std::filesystem::path >  fullPaths,
                                        std::span< SamplerManager::Handle > samplers,
                                        RgTextureSwizzling                  customPbrSwizzling )
{
    assert( fullPaths.size() == TEXTURES_PER_MATERIAL_COUNT );
    assert( samplers.size() == TEXTURES_PER_MATERIAL_COUNT );

    if( materialName.empty() )
    {
        assert( 0 );
        return false;
    }

    if( PreferExistingMaterials )
    {
        if( materials.contains( materialName ) )
        {
            debug::Warning( "{}: Material with the same name already exists. "
                            "Ignoring the new one.",
                            materialName );
            return false;
        }
    }


    // clang-format off
    TextureOverrides ovrd[] = {
        TextureOverrides( fullPaths[ 0 ], true, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( fullPaths[ 1 ], false, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( fullPaths[ 2 ], false, GetLoader( imageLoader, imageLoaderDev ) ),
    };
    static_assert( std::size( ovrd ) == TEXTURES_PER_MATERIAL_COUNT );
    // clang-format on


    std::optional< RgTextureSwizzling > swizzlings[] = {
        std::nullopt,
        std::optional( customPbrSwizzling ),
        std::nullopt,
    };
    static_assert( std::size( swizzlings ) == TEXTURES_PER_MATERIAL_COUNT );
    static_assert( MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX == 1 );


    MakeMaterial( cmd, frameIndex, materialName, ovrd, samplers, swizzlings );
    return true;
}

uint32_t TextureManager::PrepareTexture( VkCommandBuffer                                 cmd,
                                         uint32_t                                        frameIndex,
                                         const std::optional< ImageLoader::ResultInfo >& info,
                                         SamplerManager::Handle              samplerHandle,
                                         bool                                useMipmaps,
                                         const char*                         debugName,
                                         bool                                isUpdateable,
                                         std::optional< RgTextureSwizzling > swizzling,
                                         std::filesystem::path&&             filepath,
                                         std::vector< Texture >::iterator    targetSlot )
{
    if( !info )
    {
        return EMPTY_TEXTURE_INDEX;
    }


    // TODO: check if texture exists by filepath, then return ready-to-use index


    if( targetSlot == textures.end() )
    {
        // no empty slots
        debug::Warning( "Reached texture limit: {}, while uploading {}",
                        textures.size(),
                        Utils::SafeCstr( debugName ) );
        return EMPTY_TEXTURE_INDEX;
    }

    if( info->baseSize.width == 0 || info->baseSize.height == 0 )
    {
        debug::Warning( "Incorrect size ({},{}) of one of images in a texture {}",
                        info->baseSize.width,
                        info->baseSize.height,
                        Utils::SafeCstr( debugName ) );
        return EMPTY_TEXTURE_INDEX;
    }

    assert( info->dataSize > 0 );
    assert( info->levelCount > 0 && info->levelSizes[ 0 ] > 0 );

    auto uploadInfo = TextureUploader::UploadInfo{
        .cmd                    = cmd,
        .frameIndex             = frameIndex,
        .pData                  = info->pData,
        .dataSize               = info->dataSize,
        .cubemap                = {},
        .baseSize               = info->baseSize,
        .format                 = info->format,
        .useMipmaps             = useMipmaps,
        .pregeneratedLevelCount = info->isPregenerated ? info->levelCount : 0,
        .pLevelDataOffsets      = info->levelOffsets,
        .pLevelDataSizes        = info->levelSizes,
        .isUpdateable           = isUpdateable,
        .pDebugName             = debugName,
        .isCubemap              = false,
        .swizzling              = swizzling,
    };

    auto [ wasUploaded, image, view ] = textureUploader->UploadImage( uploadInfo );

    if( !wasUploaded )
    {
        return EMPTY_TEXTURE_INDEX;
    }

    // insert
    *targetSlot = Texture{
        .image         = image,
        .view          = view,
        .size          = uploadInfo.baseSize,
        .format        = uploadInfo.format,
        .samplerHandle = samplerHandle,
        .swizzling     = uploadInfo.swizzling,
        .filepath      = std::move( filepath ),
    };
    return uint32_t( std::distance( textures.begin(), targetSlot ) );
}

void TextureManager::InsertMaterial( uint32_t         frameIndex,
                                     std::string_view materialName,
                                     const Material&  material )
{
    auto [ iter, insertednew ] = materials.insert( { std::string( materialName ), material } );

    if( !insertednew )
    {
        if( PreferExistingMaterials )
        {
            assert( 0 );
        }
        else
        {
            debug::Warning( "{}: Material with the same name already exists. "
                            "Overwriting the old one",
                            iter->first );

            Material& existing = iter->second;

            // TODO: buggy
            // destroy old, overwrite with new
            DestroyMaterialTextures( frameIndex, existing );
            existing = material;
        }
    }
}

void TextureManager::DestroyMaterialTextures( uint32_t frameIndex, const Material& material )
{
    for( auto t : material.textures.indices )
    {
        if( t != EMPTY_TEXTURE_INDEX )
        {
            AddToBeDestroyed( frameIndex, textures[ t ] );
        }
    }
}

bool TextureManager::TryDestroyMaterial( uint32_t frameIndex, const char* materialName )
{
    if( Utils::IsCstrEmpty( materialName ) )
    {
        return false;
    }

    auto it = materials.find( materialName );
    if( it == materials.end() )
    {
        return false;
    }

    DestroyMaterialTextures( frameIndex, it->second );
    materials.erase( it );

    return true;
}

void TextureManager::DestroyTexture( const Texture& texture )
{
    assert( texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE );

    textureUploader->DestroyImage( texture.image, texture.view );
}

void TextureManager::AddToBeDestroyed( uint32_t frameIndex, Texture& texture )
{
    assert( texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE );

    texturesToDestroy[ frameIndex ].push_back( std::move( texture ) );

    // nullify the slot
    texture = {};
}

MaterialTextures TextureManager::GetMaterialTextures( const char* materialName ) const
{
    if( Utils::IsCstrEmpty( materialName ) )
    {
        return EmptyMaterialTextures;
    }

    const auto it = materials.find( materialName );

    if( it == materials.end() )
    {
        return EmptyMaterialTextures;
    }

    return it->second.textures;
}

VkDescriptorSet TextureManager::GetDescSet( uint32_t frameIndex ) const
{
    return textureDesc->GetDescSet( frameIndex );
}

VkDescriptorSetLayout TextureManager::GetDescSetLayout() const
{
    return textureDesc->GetDescSetLayout();
}

void TextureManager::OnFileChanged( FileType type, const std::filesystem::path& filepath )
{
    if( type == FileType::PNG || type == FileType::TGA || type == FileType::KTX2 ||
        type == FileType::JPG )
    {
        texturesToReloadNoExt.push_back(
            std::filesystem::path( filepath ).replace_extension( "" ) );
    }
}

uint32_t TextureManager::GetWaterNormalTextureIndex() const
{
    return waterNormalTextureIndex;
}

#define IF_LAYER_NOT_NULL( member, field, default )                                            \
    ( primitive.pEditorInfo                                                                    \
          ? primitive.pEditorInfo->member ? primitive.pEditorInfo->member->field : ( default ) \
          : ( default ) )

std::array< MaterialTextures, 4 > TextureManager::GetTexturesForLayers(
    const RgMeshPrimitiveInfo& primitive ) const
{
    return {
        GetMaterialTextures( primitive.pTextureName ),
        GetMaterialTextures( IF_LAYER_NOT_NULL( pLayer1, pTextureName, nullptr ) ),
        GetMaterialTextures( IF_LAYER_NOT_NULL( pLayer2, pTextureName, nullptr ) ),
        GetMaterialTextures( IF_LAYER_NOT_NULL( pLayerLightmap, pTextureName, nullptr ) ),
    };
}

std::array< RgColor4DPacked32, 4 > TextureManager::GetColorForLayers(
    const RgMeshPrimitiveInfo& primitive ) const
{
    return {
        primitive.color,
        IF_LAYER_NOT_NULL( pLayer1, color, 0xFFFFFFFF ),
        IF_LAYER_NOT_NULL( pLayer2, color, 0xFFFFFFFF ),
        IF_LAYER_NOT_NULL( pLayerLightmap, color, 0xFFFFFFFF ),
    };
}

auto TextureManager::ExportMaterialTextures( const char*                  materialName,
                                             const std::filesystem::path& folder,
                                             bool                         overwriteExisting ) const
    -> std::array< ExportResult, TEXTURES_PER_MATERIAL_COUNT >
{
    std::array< ExportResult, TEXTURES_PER_MATERIAL_COUNT > arr;

    if( folder.empty() )
    {
        assert( 0 );
        return {};
    }

    MaterialTextures mat = GetMaterialTextures( materialName );

    for( size_t i = 0; i < std::size( mat.indices ); i++ )
    {
        if( mat.indices[ i ] == EMPTY_TEXTURE_INDEX )
        {
            continue;
        }

        const Texture& info = textures[ mat.indices[ i ] ];

        if( info.image == VK_NULL_HANDLE || info.size.width == 0 || info.size.height == 0 ||
            info.format == VK_FORMAT_UNDEFINED )
        {
            continue;
        }

        auto relativeFilePath =
            TextureOverrides::GetTexturePath( "", materialName, postfixes[ i ], ".tga" );

        bool exported = TextureExporter().ExportAsTGA( *memAllocator,
                                                       *cmdManager,
                                                       info.image,
                                                       info.size,
                                                       info.format,
                                                       folder / relativeFilePath,
                                                       i == MATERIAL_ALBEDO_ALPHA_INDEX,
                                                       overwriteExisting );
        if( exported )
        {
            arr[ i ].relativePath = relativeFilePath.string();

            std::tie( arr[ i ].addressModeU, arr[ i ].addressModeV ) =
                SamplerManager::AccessAddressModes( info.samplerHandle );
        }
    }

    return arr;
}
