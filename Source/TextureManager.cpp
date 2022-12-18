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

#include <numeric>

#include "Const.h"
#include "Utils.h"
#include "TextureOverrides.h"
#include "Generated/ShaderCommonC.h"
#include "RgException.h"


using namespace RTGL1;


namespace
{

constexpr MaterialTextures                EmptyMaterialTextures = { EMPTY_TEXTURE_INDEX,
                                                     EMPTY_TEXTURE_INDEX,
                                                     EMPTY_TEXTURE_INDEX };

constexpr RgSamplerFilter                 DefaultDynamicSamplerFilter = RG_SAMPLER_FILTER_LINEAR;

template< typename T > constexpr const T* DefaultIfNull( const T* pData, const T* pDefault )
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


TextureManager::TextureManager( VkDevice                                       _device,
                                std::shared_ptr< MemoryAllocator >             _memAllocator,
                                std::shared_ptr< SamplerManager >              _samplerMgr,
                                const std::shared_ptr< CommandBufferManager >& _cmdManager,
                                std::shared_ptr< UserFileLoad >                _userFileLoad,
                                const RgInstanceCreateInfo&                    _info,
                                const LibraryConfig::Config&                   _config )
    : device( _device )
    , pbrSwizzling( _info.pbrTextureSwizzling )
    , samplerMgr( std::move( _samplerMgr ) )
    , waterNormalTextureIndex( 0 )
    , currentDynamicSamplerFilter( DefaultDynamicSamplerFilter )
    , postfixes
        {
            DefaultIfNull(_info.pOverridenAlbedoAlphaTexturePostfix, DEFAULT_TEXTURE_POSTFIX_ALBEDO_ALPHA),
            DefaultIfNull(_info.pOverridenRoughnessMetallicEmissionTexturePostfix, DEFAULT_TEXTURE_POSTFIX_ROUGNESS_METALLIC_EMISSION),
            DefaultIfNull(_info.pOverridenNormalTexturePostfix, DEFAULT_TEXTURE_POSTFIX_NORMAL),
        }
    , overridenIsSRGB
        {
            !!_info.overridenAlbedoAlphaTextureIsSRGB,
            !!_info.overridenRoughnessMetallicEmissionTextureIsSRGB,
            !!_info.overridenNormalTextureIsSRGB,
        }
    , originalIsSRGB
        {
            !!_info.originalAlbedoAlphaTextureIsSRGB,
            !!_info.originalRoughnessMetallicEmissionTextureIsSRGB,
            !!_info.originalNormalTextureIsSRGB,
        }
    , forceNormalMapFilterLinear( !!_info.textureSamplerForceNormalMapFilterLinear )
{
    const uint32_t maxTextureCount =
        std::clamp( _info.maxTextureCount, TEXTURE_COUNT_MIN, TEXTURE_COUNT_MAX );

    imageLoader = std::make_shared< ImageLoader >( std::move( _userFileLoad ) );

    if( _config.developerMode )
    {
        imageLoaderDev = std::make_shared< ImageLoaderDev >( imageLoader );
    }


    textureDesc = std::make_shared< TextureDescriptors >(
        device, samplerMgr, maxTextureCount, BINDING_TEXTURES );
    textureUploader = std::make_shared< TextureUploader >( device, std::move( _memAllocator ) );

    textures.resize( maxTextureCount );

    // submit cmd to create empty texture
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    {
        CreateEmptyTexture( cmd, 0 );
        CreateWaterNormalTexture( cmd, 0, _info.pWaterNormalTexturePath );
    }
    _cmdManager->Submit( cmd );
    _cmdManager->WaitGraphicsIdle();

    if( this->waterNormalTextureIndex == EMPTY_TEXTURE_INDEX )
    {
        throw RgException( RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES,
                           "Couldn't create water normal texture with path: " +
                               std::string( _info.pWaterNormalTexturePath ) );
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
void TextureManager::CreateWaterNormalTexture( VkCommandBuffer cmd,
                                               uint32_t        frameIndex,
                                               const char*     pFilePath )
{
    if( Utils::IsCstrEmpty( pFilePath ) )
    {
        this->waterNormalTextureIndex = EMPTY_TEXTURE_INDEX;
        return;
    }

    constexpr uint32_t   defaultData[] = { 0x7F7FFFFF };
    constexpr RgExtent2D defaultSize   = { 1, 1 };

    // try to load image file
    TextureOverrides ovrd( {} /* absolute path */,
                           pFilePath,
                           "",
                           defaultData,
                           defaultSize,
                           VK_FORMAT_R8G8B8A8_UNORM,
                           imageLoader.get() );

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
        throw RgException(
            RG_RESULT_WRONG_FUNCTION_ARGUMENT,
            R"('pTextureName' must be not null or empty string in RgOriginalTextureInfo)" );
    }

    if( info.pPixels == nullptr )
    {
        throw RgException(
            RG_RESULT_WRONG_FUNCTION_ARGUMENT,
            R"('pPixels' must be not null in RgOriginalTextureInfo)" );
    }

    const auto samplerHandle =
        SamplerManager::Handle( info.filter, info.addressModeU, info.addressModeV );

    const auto normalMapSamplerHandle =
        SamplerManager::Handle( forceNormalMapFilterLinear ? RG_SAMPLER_FILTER_LINEAR : info.filter,
                                info.addressModeU,
                                info.addressModeV );


    // clang-format off
    TextureOverrides ovrd[] = {
        TextureOverrides( folder, info.pTextureName, postfixes[ 0 ], info.pPixels, info.size, VK_FORMAT_R8G8B8A8_SRGB, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( folder, info.pTextureName, postfixes[ 1 ], nullptr, {}, VK_FORMAT_R8G8B8A8_UNORM, GetLoader( imageLoader, imageLoaderDev ) ),
        TextureOverrides( folder, info.pTextureName, postfixes[ 2 ], nullptr, {}, VK_FORMAT_R8G8B8A8_UNORM, GetLoader( imageLoader, imageLoaderDev ) ),
    };
    static_assert( std::size( ovrd ) == TEXTURES_PER_MATERIAL_COUNT );
    // clang-format on


    constexpr bool isUpdateable = false;


    MaterialTextures mtextures = {};
    for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
    {
        mtextures.indices[ i ] = PrepareTexture(
            cmd,
            frameIndex,
            ovrd[ i ].result,
            i == MATERIAL_NORMAL_INDEX ? normalMapSamplerHandle : samplerHandle,
            true,
            ovrd[ i ].debugname,
            isUpdateable,
            i == MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX ? std::optional( pbrSwizzling )
                                                            : std::nullopt,
            std::move( ovrd[ i ].path ),
            FindEmptySlot( textures ) );
    }

    InsertMaterial( frameIndex,
                    info.pTextureName,
                    Material{
                        .textures     = mtextures,
                        .isUpdateable = isUpdateable,
                    } );
    
    return true;
}

/*bool TextureManager::UpdateMaterial( VkCommandBuffer cmd, const RgMaterialUpdateInfo& updateInfo )
{
    const auto it = materials.find( updateInfo.target );

    // must exist
    if( it == materials.end() )
    {
        throw RgException( RG_CANT_UPDATE_MATERIAL,
                           "Material with ID=" + std::to_string( updateInfo.target ) +
                               " was not created" );
    }

    // must be updateable
    if( !it->second.isUpdateable )
    {
        throw RgException( RG_CANT_UPDATE_MATERIAL,
                           "Material with ID=" + std::to_string( updateInfo.target ) +
                               " was not marked as updateable" );
    }

    const void* updateData[ TEXTURES_PER_MATERIAL_COUNT ] = {
        updateInfo.textures.pDataAlbedoAlpha,
        updateInfo.textures.pDataRoughnessMetallicEmission,
        updateInfo.textures.pDataNormal,
    };

    auto& textureIndices = it->second.textures.indices;
    static_assert( sizeof( textureIndices ) / sizeof( textureIndices[ 0 ] ) ==
                   TEXTURES_PER_MATERIAL_COUNT );

    bool wasUpdated = false;

    for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
    {
        uint32_t textureIndex = textureIndices[ i ];

        if( textureIndex == EMPTY_TEXTURE_INDEX )
        {
            continue;
        }

        VkImage img = textures[ textureIndex ].image;

        if( img == VK_NULL_HANDLE || updateData[ i ] == nullptr )
        {
            continue;
        }

        textureUploader->UpdateImage( cmd, img, updateData[ i ] );
        wasUpdated = true;
    }

    return wasUpdated;
}*/

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

    if( targetSlot == textures.end() )
    {
        // no empty slots
        // TODO: print warning
        assert( 0 && "Too many textures" );
        return EMPTY_TEXTURE_INDEX;
    }

    if( info->baseSize.width == 0 || info->baseSize.height == 0 )
    {
        using namespace std::string_literals;

        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "Incorrect size (" + std::to_string( info->baseSize.width ) + ", " +
                               std::to_string( info->baseSize.height ) +
                               ") of one of images in a material" +
                               ( debugName != nullptr ? " with name: "s + debugName : ""s ) );
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
        .format        = uploadInfo.format,
        .samplerHandle = samplerHandle,
        .swizzling     = uploadInfo.swizzling,
        .filepath      = std::move( filepath ),
    };
    return uint32_t( std::distance( textures.begin(), targetSlot ) );
}

/*uint32_t TextureManager::CreateAnimatedMaterial( VkCommandBuffer                     cmd,
                                                 uint32_t                            frameIndex,
                                                 const RgAnimatedMaterialCreateInfo& createInfo )
{
    if( createInfo.frameCount == 0 )
    {
        return RG_NO_MATERIAL;
    }

    std::vector< uint32_t > materialIndices( createInfo.frameCount );

    // animated material is a series of static materials
    for( uint32_t i = 0; i < createInfo.frameCount; i++ )
    {
        materialIndices[ i ] = CreateMaterial( cmd, frameIndex, createInfo.pFrames[ i ] );
    }

    return InsertAnimatedMaterial( materialIndices );
}

bool TextureManager::ChangeAnimatedMaterialFrame( uint32_t animMaterial, uint32_t materialFrame )
{
    const auto animIt = animatedMaterials.find( animMaterial );

    if( animIt == animatedMaterials.end() )
    {
        throw RgException( RG_CANT_UPDATE_ANIMATED_MATERIAL,
                           "Material with ID=" + std::to_string( animMaterial ) +
                               " is not animated" );
    }

    AnimatedMaterial& anim = animIt->second;

    {
        auto maxFrameCount = static_cast< uint32_t >( anim.materialIndices.size() );
        if( materialFrame >= maxFrameCount )
        {
            throw RgException( RG_CANT_UPDATE_ANIMATED_MATERIAL,
                               "Animated material with ID=" + std::to_string( animMaterial ) +
                                   " has only " + std::to_string( maxFrameCount ) +
                                   " frames, but frame with index " +
                                   std::to_string( materialFrame ) + " was requested" );
        }
    }

    anim.currentFrame = materialFrame;

    // notify subscribers
    for( auto& ws : subscribers )
    {
        // if subscriber still exist
        if( auto s = ws.lock() )
        {
            uint32_t frameMatIndex = anim.materialIndices[ anim.currentFrame ];

            // find MaterialTextures
            auto     it = materials.find( frameMatIndex );

            if( it != materials.end() )
            {
                s->OnMaterialChange( animMaterial, it->second.textures );
            }
        }
    }

    return true;
}

uint32_t TextureManager::InsertAnimatedMaterial( std::vector< uint32_t >& materialIndices )
{
    bool isEmpty = true;

    for( uint32_t m : materialIndices )
    {
        if( m != RG_NO_MATERIAL )
        {
            isEmpty = false;
            break;
        }
    }

    if( isEmpty )
    {
        return RG_NO_MATERIAL;
    }

    uint32_t animMatIndex             = GenerateMaterialIndex( materialIndices );
    animatedMaterials[ animMatIndex ] = AnimatedMaterial{
        .materialIndices = std::move( materialIndices ),
        .currentFrame    = 0,
    };

    return animMatIndex;
}*/

void TextureManager::InsertMaterial( uint32_t        frameIndex,
                                     const char*     pTextureName,
                                     const Material& material )
{
    auto [ iter, insertednew ] = materials.insert( { std::string( pTextureName ), material } );

    if( !insertednew )
    {
        Material& existing = iter->second;

        // destroy old, overwrite with new
        DestroyMaterialTextures( frameIndex, existing );
        existing = material;

        // notify subscribers
        for( auto& ws : subscribers )
        {
            if( auto s = ws.lock() )
            {
                // TODO: IMaterialDependency
                // s->OnMaterialChange(   );
            }
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

bool TextureManager::TryDestroyMaterial( uint32_t frameIndex, const char* pTextureName )
{
    if( Utils::IsCstrEmpty( pTextureName ) )
    {
        return false;
    }

    auto it = materials.find( pTextureName );
    if( it == materials.end() )
    {
        return false;
    }

    DestroyMaterialTextures( frameIndex, it->second );
    materials.erase( it );

    /*if( observer )
    {
        observer->Remove( materialIndex );
    }*/

    // notify subscribers
    for( auto& ws : subscribers )
    {
        if( auto s = ws.lock() )
        {
            // send them empty texture indices as material is destroyed

            // TODO: IMaterialDependency
            // s->OnMaterialChange( materialIndex, EmptyMaterialTextures );
        }
    }

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

MaterialTextures TextureManager::GetMaterialTextures( const char* pTextureName ) const
{
    if( Utils::IsCstrEmpty( pTextureName ) )
    {
        return EmptyMaterialTextures;
    }

    /*const auto animIt = animatedMaterials.find( materialIndex );

    if( animIt != animatedMaterials.end() )
    {
        const AnimatedMaterial& anim = animIt->second;

        // return material textures of the current frame
        return GetMaterialTextures( anim.materialIndices[ anim.currentFrame ] );
    }*/

    const auto it = materials.find( pTextureName );

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

void TextureManager::Subscribe( const std::shared_ptr< IMaterialDependency >& subscriber )
{
    subscribers.emplace_back( subscriber );
}

void TextureManager::OnFileChanged( FileType type, const std::filesystem::path& filepath )
{
    if( type == FileType::PNG || type == FileType::TGA || type == FileType::KTX2 || type == FileType::JPG )
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
