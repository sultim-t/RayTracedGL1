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

#include "Common.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "IFramebuffersDependency.h"
#include "LensFlares.h"
#include "RasterizedDataCollector.h"
#include "RasterizerPipelines.h"
#include "RasterPass.h"
#include "RenderCubemap.h"
#include "SwapchainPass.h"
#include "Tonemapping.h"
#include "Volumetric.h"
#include "RTGL1/RTGL1.h"


namespace RTGL1
{

class RenderResolutionHelper;
struct RasterDrawParams;


// This class provides rasterization functionality
class Rasterizer final
    : public IShaderDependency
    , public IFramebuffersDependency
{
public:
    explicit Rasterizer( VkDevice                                device,
                         VkPhysicalDevice                        physDevice,
                         const ShaderManager&                    shaderManager,
                         std::shared_ptr< TextureManager >       textureManager,
                         const GlobalUniform&                    uniform,
                         const SamplerManager&                   samplerManager,
                         const Tonemapping&                      tonemapping,
                         const Volumetric&                       volumetric,
                         std::shared_ptr< MemoryAllocator >      allocator,
                         std::shared_ptr< Framebuffers >         storageFramebuffers,
                         std::shared_ptr< CommandBufferManager > cmdManager,
                         const RgInstanceCreateInfo&             instanceInfo );
    ~Rasterizer() override;

    Rasterizer( const Rasterizer& other )                = delete;
    Rasterizer( Rasterizer&& other ) noexcept            = delete;
    Rasterizer& operator=( const Rasterizer& other )     = delete;
    Rasterizer& operator=( Rasterizer&& other ) noexcept = delete;

    void PrepareForFrame( uint32_t frameIndex );
    void Upload( uint32_t                   frameIndex,
                 GeometryRasterType         rasterType,
                 const RgTransform&         transform,
                 const RgMeshPrimitiveInfo& info,
                 const float*               pViewProjection,
                 const RgViewport*          pViewport );
    void UploadLensFlare( uint32_t                     frameIndex,
                          const RgLensFlareUploadInfo& info,
                          float                        emissiveMult,
                          const TextureManager&        textureManager );
    void SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex );

    void DrawSkyToCubemap( VkCommandBuffer       cmd,
                           uint32_t              frameIndex,
                           const TextureManager& textureManager,
                           const GlobalUniform&  uniform );

    void DrawSkyToAlbedo( VkCommandBuffer               cmd,
                          uint32_t                      frameIndex,
                          const TextureManager&         textureManager,
                          const float*                  view,
                          const RgFloat3D&              skyViewerPos,
                          const float*                  proj,
                          const RgFloat2D&              jitter,
                          const RenderResolutionHelper& renderResolution );

    void DrawToFinalImage( VkCommandBuffer               cmd,
                           uint32_t                      frameIndex,
                           const TextureManager&         textureManager,
                           const GlobalUniform&          uniform,
                           const Tonemapping&            tonemapping,
                           const Volumetric&             volumetric,
                           const float*                  view,
                           const float*                  proj,
                           const RgFloat2D&              jitter,
                           const RenderResolutionHelper& renderResolution );

    void DrawToSwapchain( VkCommandBuffer       cmd,
                          uint32_t              frameIndex,
                          FramebufferImageIndex imageToDrawIn,
                          const TextureManager& textureManager,
                          const float*          view,
                          const float*          proj,
                          uint32_t              swapchainWidth,
                          uint32_t              swapchainHeight );

    void OnShaderReload( const ShaderManager* shaderManager ) override;
    void OnFramebuffersSizeChange( const ResolutionState& resolutionState ) override;

    const std::shared_ptr< RenderCubemap >& GetRenderCubemap() const;

    uint32_t GetLensFlareCullingInputCount() const;

private:
    void Draw( VkCommandBuffer cmd, uint32_t frameIndex, const RasterDrawParams& drawParams );

    void CreatePipelineLayouts( VkDescriptorSetLayout* allLayouts,
                                size_t                 count,
                                VkDescriptorSetLayout  texturesSetLayout );

private:
    VkDevice         device;
    VkPipelineLayout rasterPassPipelineLayout;
    VkPipelineLayout swapchainPassPipelineLayout;

    std::shared_ptr< MemoryAllocator >      allocator;
    std::shared_ptr< CommandBufferManager > cmdManager;
    std::shared_ptr< Framebuffers >         storageFramebuffers;

    std::shared_ptr< RasterPass >    rasterPass;
    std::shared_ptr< SwapchainPass > swapchainPass;

    std::shared_ptr< RasterizedDataCollector > collector;

    std::shared_ptr< RenderCubemap > renderCubemap;

    std::unique_ptr< LensFlares > lensFlares;
};

}
