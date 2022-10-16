// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "CubemapManager.h"

#include "Generated/ShaderCommonC.h"
#include "Const.h"
#include "TextureOverrides.h"
#include "RgException.h"

namespace
{

constexpr uint32_t MAX_CUBEMAP_COUNT = 32;

// use albedo-alpha texture data
constexpr uint32_t MATERIAL_COLOR_TEXTURE_INDEX = 0;
static_assert( MATERIAL_COLOR_TEXTURE_INDEX < RTGL1::TEXTURES_PER_MATERIAL_COUNT );

template< typename T > constexpr const T* DefaultIfNull( const T* pData, const T* pDefault )
{
    return pData != nullptr ? pData : pDefault;
}

void CheckIfFaceCorrect( const RTGL1::ImageLoader::ResultInfo& face,
                         RgExtent2D                            commonSize,
                         VkFormat                              commonFormat,
                         const char*                           pDebugName )
{
    using namespace std::string_literals;

    assert( face.pData != nullptr );
    const auto& sz = face.baseSize;

    if( face.format != commonFormat )
    {
        throw RTGL1::RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                                  "Cubemap must have the same format on each face. Failed on: "s +
                                      pDebugName );
    }

    if( sz.width != sz.height )
    {
        throw RTGL1::RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                                  "Cubemap must have square face size: "s + pDebugName + " has (" +
                                      std::to_string( sz.width ) + ", " +
                                      std::to_string( sz.height ) + ")" );
    }

    if( sz.width != commonSize.width || sz.height != commonSize.height )
    {
        throw RTGL1::RgException(
            RG_RESULT_WRONG_FUNCTION_ARGUMENT,
            "Cubemap faces must have the same size: "s + pDebugName + " has (" +
                std::to_string( sz.width ) + ", " + std::to_string( sz.height ) +
                ")"
                "but expected (" +
                std::to_string( commonSize.width ) + ", " + std::to_string( commonSize.height ) +
                ") like on " + pDebugName );
    }
}

}


RTGL1::CubemapManager::CubemapManager( VkDevice                           _device,
                                       std::shared_ptr< MemoryAllocator > _allocator,
                                       std::shared_ptr< SamplerManager >  _samplerManager,
                                       const std::shared_ptr< CommandBufferManager >& _cmdManager,
                                       std::shared_ptr< UserFileLoad >                _userFileLoad,
                                       const RgInstanceCreateInfo&                    _info,
                                       const LibraryConfig::Config&                   _config )
    : device( _device )
    , allocator( std::move( _allocator ) )
    , samplerManager( std::move( _samplerManager ) )
    , cubemaps( MAX_CUBEMAP_COUNT )
    , defaultTexturesPath(
          DefaultIfNull( _info.pOverridenTexturesFolderPath, DEFAULT_TEXTURES_PATH ) )
    , overridenTexturePostfix( DefaultIfNull( _info.pOverridenAlbedoAlphaTexturePostfix,
                                              DEFAULT_TEXTURE_POSTFIX_ALBEDO_ALPHA ) )
{
    if( _config.developerMode && _info.pOverridenTexturesFolderPathDeveloper != nullptr )
    {
        defaultTexturesPath = _info.pOverridenTexturesFolderPathDeveloper;
    }

    imageLoader = std::make_shared< ImageLoader >( std::move( _userFileLoad ) );
    cubemapDesc = std::make_shared< TextureDescriptors >(
        device, samplerManager, MAX_CUBEMAP_COUNT, BINDING_CUBEMAPS );
    cubemapUploader = std::make_shared< CubemapUploader >( device, allocator );

    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    {
        CreateEmptyCubemap( cmd );
    }
    _cmdManager->Submit( cmd );
    _cmdManager->WaitGraphicsIdle();
}

void RTGL1::CubemapManager::CreateEmptyCubemap( VkCommandBuffer cmd )
{
    const uint32_t        whitePixel       = 0xFFFFFFFF;
    const char*           emptyTextureName = "_RTGL1DefaultCubemap";

    RgOriginalCubemapInfo info = {
        .pTextureName     = emptyTextureName,
        .pPixelsPositiveX = &whitePixel,
        .pPixelsNegativeX = &whitePixel,
        .pPixelsPositiveY = &whitePixel,
        .pPixelsNegativeY = &whitePixel,
        .pPixelsPositiveZ = &whitePixel,
        .pPixelsNegativeZ = &whitePixel,
        .sideSize         = 1,
    };

    bool b = TryCreateCubemap( cmd, 0, info );

    assert( b );
    assert( cubemaps.contains( emptyTextureName ) );

    cubemapDesc->SetEmptyTextureInfo( cubemaps[ emptyTextureName ].view );
}

RTGL1::CubemapManager::~CubemapManager()
{
    for( const auto& [ name, t ] : cubemaps )
    {
        assert( ( t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE ) ||
                ( t.image != VK_NULL_HANDLE && t.view != VK_NULL_HANDLE ) );

        if( t.image != VK_NULL_HANDLE )
        {
            cubemapUploader->DestroyImage( t.image, t.view );
        }
    }

    for( const std::vector< Texture >& vec : cubemapsToDestroy )
    {
        for( const auto& t : vec )
        {
            assert( ( t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE ) ||
                    ( t.image != VK_NULL_HANDLE && t.view != VK_NULL_HANDLE ) );

            if( t.image != VK_NULL_HANDLE )
            {
                cubemapUploader->DestroyImage( t.image, t.view );
            }
        }
    }
}

bool RTGL1::CubemapManager::TryCreateCubemap( VkCommandBuffer              cmd,
                                              uint32_t                     frameIndex,
                                              const RgOriginalCubemapInfo& info )
{
    TextureUploader::UploadInfo upload = {
        .cmd          = cmd,
        .frameIndex   = frameIndex,
        .useMipmaps   = true,
        .isUpdateable = false,
        .pDebugName   = nullptr,
        .isCubemap    = true,
    };

    // must be '0' to use special TextureOverrides constructor
    static_assert( MATERIAL_COLOR_TEXTURE_INDEX == 0 );

    TextureOverrides::OverrideInfo parseInfo = {
        .commonFolderPath = defaultTexturesPath.c_str(),
        .postfixes        = { overridenTexturePostfix.c_str(), "", "" },
        .overridenIsSRGB  = { true, false, false },
        .originalIsSRGB   = { true, false, false },
    };
    auto*       ldr = imageLoader.get();

    RgExtent2D  size = { info.sideSize, info.sideSize };

    std::string faceNames[] = {
        std::string( info.pTextureName ) + "_px", std::string( info.pTextureName ) + "_nx",
        std::string( info.pTextureName ) + "_py", std::string( info.pTextureName ) + "_ny",
        std::string( info.pTextureName ) + "_pz", std::string( info.pTextureName ) + "_nz",
    };
    const void* facePixels[] = {
        info.pPixelsPositiveX, info.pPixelsNegativeX, info.pPixelsPositiveY,
        info.pPixelsNegativeY, info.pPixelsPositiveZ, info.pPixelsNegativeZ,
    };

    // load additional textures, they'll be freed after leaving the scope
    TextureOverrides ovrd[] = {
        TextureOverrides( faceNames[ 0 ].c_str(), facePixels[ 0 ], size, parseInfo, ldr ),
        TextureOverrides( faceNames[ 1 ].c_str(), facePixels[ 1 ], size, parseInfo, ldr ),
        TextureOverrides( faceNames[ 2 ].c_str(), facePixels[ 2 ], size, parseInfo, ldr ),
        TextureOverrides( faceNames[ 3 ].c_str(), facePixels[ 3 ], size, parseInfo, ldr ),
        TextureOverrides( faceNames[ 4 ].c_str(), facePixels[ 4 ], size, parseInfo, ldr ),
        TextureOverrides( faceNames[ 5 ].c_str(), facePixels[ 5 ], size, parseInfo, ldr ),
    };

    // all overrides must have albedo data and the same and square size
    bool       useOvrd = true;


    RgExtent2D commonSize   = {};
    VkFormat   commonFormat = VK_FORMAT_UNDEFINED;
    {
        if( const auto& firstAlbedo = ovrd[ 0 ].GetResult( MATERIAL_COLOR_TEXTURE_INDEX ) )
        {
            commonSize   = { firstAlbedo->baseSize.width, firstAlbedo->baseSize.height };
            commonFormat = firstAlbedo->format;
        }
        else
        {
            useOvrd = false;
        }
    }


    // check if all entries are correct
    for( auto& o : ovrd )
    {
        if( const auto& face = o.GetResult( MATERIAL_COLOR_TEXTURE_INDEX ) )
        {
            CheckIfFaceCorrect( *face, commonSize, commonFormat, o.GetDebugName() );
        }
        else
        {
            useOvrd = false;
        }
    }


    if( useOvrd )
    {
        upload.pDebugName = ovrd[ 0 ].GetDebugName();

        for( uint32_t i = 0; i < 6; i++ )
        {
            upload.cubemap.pFaces[ i ] = ovrd[ i ].GetResult( MATERIAL_COLOR_TEXTURE_INDEX )->pData;
        }
    }
    else
    {
        // use data provided by user
        commonSize   = { info.sideSize, info.sideSize };
        commonFormat = VK_FORMAT_R8G8B8A8_SRGB;


        if( info.sideSize == 0 )
        {
            throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                               "Cubemap's side size must be non-zero" );
        }

        for( uint32_t i = 0; i < 6; i++ )
        {
            // if original data is not valid
            if( facePixels[ i ] == nullptr )
            {
                return false;
            }

            upload.cubemap.pFaces[ i ] = facePixels[ i ];
        }
    }



    // TODO: KTX cubemap image uploading with proper formats
    upload.format = commonFormat;
    if( commonFormat != VK_FORMAT_R8G8B8A8_SRGB && commonFormat != VK_FORMAT_R8G8B8A8_UNORM )
    {
        assert( false && "For now, cubemaps only support only R8G8B8A8 formats!" );
        return false;
    }
    upload.baseSize = commonSize;
    upload.dataSize = 4 * commonSize.width * commonSize.height;
    //


    auto i = cubemapUploader->UploadImage( upload );
    if( !i.wasUploaded )
    {
        assert( false );
        return false;
    }

    Texture txd = {
        .image         = i.image,
        .view          = i.view,
        .samplerHandle = SamplerManager::Handle( RG_SAMPLER_FILTER_LINEAR,
                                                 RG_SAMPLER_ADDRESS_MODE_CLAMP,
                                                 RG_SAMPLER_ADDRESS_MODE_CLAMP ),
    };

    auto [ iter, insertednew ] = cubemaps.insert( { std::string( info.pTextureName ), txd } );

    if( !insertednew )
    {
        Texture& existing = iter->second;

        // destroy old, overwrite with new
        AddForDeletion( frameIndex, existing );
        existing = txd;
    }

    return true;
}

void RTGL1::CubemapManager::AddForDeletion( uint32_t frameIndex, Texture& txd )
{
    assert( txd.image != VK_NULL_HANDLE );
    assert( txd.view != VK_NULL_HANDLE );

    // destroy later
    cubemapsToDestroy[ frameIndex ].push_back( txd );

    // nullify
    txd = {
        .image         = VK_NULL_HANDLE,
        .view          = VK_NULL_HANDLE,
        .samplerHandle = SamplerManager::Handle(),
    };
}

bool RTGL1::CubemapManager::TryDestroyCubemap( uint32_t frameIndex, const char* pTextureName )
{
    if( pTextureName == nullptr )
    {
        return false;
    }

    auto it = cubemaps.find( pTextureName );
    if( it == cubemaps.end() )
    {
        return false;
    }

    AddForDeletion( frameIndex, it->second );
    cubemaps.erase( it );

    return true;
}

VkDescriptorSetLayout RTGL1::CubemapManager::GetDescSetLayout() const
{
    return cubemapDesc->GetDescSetLayout();
}

VkDescriptorSet RTGL1::CubemapManager::GetDescSet( uint32_t frameIndex ) const
{
    return cubemapDesc->GetDescSet( frameIndex );
}

void RTGL1::CubemapManager::PrepareForFrame( uint32_t frameIndex )
{
    // destroy delayed textures
    for( const Texture& t : cubemapsToDestroy[ frameIndex ] )
    {
        vkDestroyImage( device, t.image, nullptr );
        vkDestroyImageView( device, t.view, nullptr );
    }
    cubemapsToDestroy[ frameIndex ].clear();

    // clear staging buffer that are not in use
    cubemapUploader->ClearStaging( frameIndex );
}

void RTGL1::CubemapManager::SubmitDescriptors( uint32_t frameIndex )
{
    // update desc set with current values
    uint32_t iter = 0;
    for( const auto& [ name, cubetxd ] : cubemaps )
    {
        if( cubetxd.image != VK_NULL_HANDLE )
        {
            cubemapDesc->UpdateTextureDesc( frameIndex, iter, cubetxd.view, cubetxd.samplerHandle );
        }
        else
        {
            // reset descriptor to empty texture
            cubemapDesc->ResetTextureDesc( frameIndex, iter );
        }

        iter++;
    }

    cubemapDesc->FlushDescWrites();
}

uint32_t RTGL1::CubemapManager::TryGetDescriptorIndex( const char* pTextureName )
{
    // TODO: TextureDescriptors should return an index on creation,
    //       now, iter must be the same as in SubmitDescriptors

    uint32_t iter = 0;
    for( const auto& [ name, cubetxd ] : cubemaps )
    {
        if( cubetxd.view != VK_NULL_HANDLE )
        {
            if( name == pTextureName)
            {
                return iter;
            }
        }

        iter++;
    }

    // TODO: must not happen for now
    assert( false );
    return 0;
}

bool RTGL1::CubemapManager::IsCubemapValid( uint32_t cubemapIndex ) const
{
    return cubemapIndex < MAX_CUBEMAP_COUNT;
}
