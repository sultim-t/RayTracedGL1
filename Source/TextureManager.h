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

#pragma once

#include <array>
#include <list>
#include <string>

#include "Common.h"
#include "CommandBufferManager.h"
#include "Material.h"
#include "ImageLoader.h"
#include "ImageLoaderDev.h"
#include "IMaterialDependency.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"
#include "TextureDescriptors.h"
#include "TextureUploader.h"
#include "LibraryConfig.h"
#include "TextureObserver.h"

namespace RTGL1
{

class TextureManager
{
public:
    TextureManager( VkDevice                                       device,
                    std::shared_ptr< MemoryAllocator >             memAllocator,
                    std::shared_ptr< SamplerManager >              samplerManager,
                    const std::shared_ptr< CommandBufferManager >& cmdManager,
                    std::shared_ptr< UserFileLoad >                userFileLoad,
                    const RgInstanceCreateInfo&                    info,
                    const LibraryConfig::Config&                   config );
    ~TextureManager();

    TextureManager( const TextureManager& other )     = delete;
    TextureManager( TextureManager&& other ) noexcept = delete;
    TextureManager&       operator=( const TextureManager& other ) = delete;
    TextureManager&       operator=( TextureManager&& other ) noexcept = delete;

    void                  PrepareForFrame( uint32_t frameIndex );
    void                  SubmitDescriptors( uint32_t                         frameIndex,
                                             const RgDrawFrameTexturesParams* pTexturesParams,
                                             bool                             forceUpdateAllDescriptors =
                                                 false /* true, if mip lod bias was changed, for example */ );
    VkDescriptorSet       GetDescSet( uint32_t frameIndex ) const;
    VkDescriptorSetLayout GetDescSetLayout() const;


    bool                  TryCreateMaterial( VkCommandBuffer              cmd,
                                             uint32_t                     frameIndex,
                                             const RgOriginalTextureInfo& info );
    bool                  TryDestroyMaterial( uint32_t frameIndex, const char* pTextureName );


    MaterialTextures      GetMaterialTextures( const char* pTextureName ) const;
    std::array< MaterialTextures, 4 > GetTexturesForLayers(
        const RgMeshPrimitiveInfo& primitive ) const;
    std::array< RgColor4DPacked32, 4 > GetColorForLayers(
        const RgMeshPrimitiveInfo& primitive ) const;
    uint32_t GetWaterNormalTextureIndex() const;


    // Subscribe to material change event.
    // shared_ptr will be transformed to weak_ptr
    void     Subscribe( std::shared_ptr< IMaterialDependency > subscriber );
    void     Unsubscribe( const IMaterialDependency* subscriber );

private:
    struct Material
    {
        MaterialTextures textures;
        bool             isUpdateable;
    };

private:
    void     CreateEmptyTexture( VkCommandBuffer cmd, uint32_t frameIndex );
    void     CreateWaterNormalTexture( VkCommandBuffer cmd,
                                       uint32_t        frameIndex,
                                       const char*     pFilePath );

    uint32_t PrepareTexture( VkCommandBuffer                                 cmd,
                             uint32_t                                        frameIndex,
                             const std::optional< ImageLoader::ResultInfo >& info,
                             SamplerManager::Handle                          samplerHandle,
                             bool                                            useMipmaps,
                             const char*                                     debugName,
                             bool                                            isUpdateable,
                             std::optional< RgTextureSwizzling >             swizzling );

    uint32_t InsertTexture( uint32_t               frameIndex,
                            VkImage                image,
                            VkImageView            view,
                            SamplerManager::Handle samplerHandle );
    void     DestroyTexture( const Texture& texture );
    void     AddToBeDestroyed( uint32_t frameIndex, const Texture& texture );

    void InsertMaterial( uint32_t frameIndex, const char* pTextureName, const Material& material );
    void DestroyMaterialTextures( uint32_t frameIndex, const Material& material );

private:
    VkDevice                                    device;
    RgTextureSwizzling                          pbrSwizzling;

    std::shared_ptr< ImageLoader >              imageLoader;
    std::shared_ptr< ImageLoaderDev >           imageLoaderDev;

    std::shared_ptr< SamplerManager >           samplerMgr;
    std::shared_ptr< TextureDescriptors >       textureDesc;
    std::shared_ptr< TextureUploader >          textureUploader;

    std::vector< Texture >                      textures;
    // Textures are not destroyed immediately, but only when they are not in use anymore
    std::vector< Texture >                      texturesToDestroy[ MAX_FRAMES_IN_FLIGHT ];

    // TODO: string keys pool
    rgl::unordered_map< std::string, Material > materials;

    uint32_t                                    waterNormalTextureIndex;

    RgSamplerFilter                             currentDynamicSamplerFilter;

    std::string                                 defaultTexturesPath;
    std::string                                 postfixes[ TEXTURES_PER_MATERIAL_COUNT ];
    bool                                        overridenIsSRGB[ TEXTURES_PER_MATERIAL_COUNT ];
    bool                                        originalIsSRGB[ TEXTURES_PER_MATERIAL_COUNT ];

    bool                                        forceNormalMapFilterLinear;

    std::list< std::weak_ptr< IMaterialDependency > > subscribers;
};

}