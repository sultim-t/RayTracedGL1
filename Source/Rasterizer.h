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

#include <vector>

#include "Common.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "IFramebuffersDependency.h"
#include "ISwapchainDependency.h"
#include "LensFlares.h"
#include "RasterizedDataCollector.h"
#include "RasterizerPipelines.h"
#include "RasterPass.h"
#include "RenderCubemap.h"
#include "ShaderManager.h"
#include "SwapchainPass.h"
#include "RTGL1/RTGL1.h"

namespace RTGL1
{


class RenderResolutionHelper;


// This class provides rasterization functionality
class Rasterizer : public ISwapchainDependency, public IShaderDependency, public IFramebuffersDependency
{
public:
    explicit Rasterizer(
        VkDevice device,
        VkPhysicalDevice physDevice,
        const std::shared_ptr<ShaderManager> &shaderManager,
        const std::shared_ptr<TextureManager> &textureManager,    
        const std::shared_ptr<GlobalUniform> &uniform,
        const std::shared_ptr<SamplerManager> &samplerManager,
        std::shared_ptr<MemoryAllocator> allocator,
        std::shared_ptr<Framebuffers> storageFramebuffers,
        std::shared_ptr<CommandBufferManager> cmdManager,
        VkFormat surfaceFormat,
        const RgInstanceCreateInfo &instanceInfo);
    ~Rasterizer() override;

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex, bool requestRasterizedSkyGeometryReuse);
    void Upload(uint32_t frameIndex, 
                const RgRasterizedGeometryUploadInfo &uploadInfo, 
                const float *viewProjection, const RgViewport *viewport);
    void UploadLensFlare(uint32_t frameIndex, const RgLensFlareUploadInfo &uploadInfo);

    void SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void DrawSkyToCubemap(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, const std::shared_ptr<GlobalUniform> &uniform);
    void DrawSkyToAlbedo(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, const float skyViewerPos[3], float *proj, const RgFloat2D &jitter, const RenderResolutionHelper &renderResolution);
    void DrawToFinalImage(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj, bool werePrimaryTraced);
    void DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj);

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;
    
    void OnShaderReload(const ShaderManager *shaderManager) override;
    void OnFramebuffersSizeChange(uint32_t width, uint32_t height) override;

    const std::shared_ptr<RenderCubemap> &GetRenderCubemap() const;

    uint32_t GetLensFlareCullingInputCount() const;

private:
    struct DrawParams
    {
        const std::shared_ptr<RasterizerPipelines> &pipelines;
        const std::vector<RasterizedDataCollector::DrawInfo> &drawInfos;
        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        uint32_t width;
        uint32_t height;
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        VkDescriptorSet texturesDescSet;
        float *defaultViewProj;
        // not the best way to optionally draw lens flares
        LensFlares *pLensFlares;
    };

private:
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex, const DrawParams &drawParams);

    void CreatePipelineLayout(VkDescriptorSetLayout texturesSetLayout);

    // If info's viewport is not the same as current one, new VkViewport will be set.
    void SetViewportIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info,  
                          const VkViewport &defaultViewport, VkViewport &curViewport);

    void BindPipelineIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, 
                           const std::shared_ptr<RasterizerPipelines> &pipelines, VkPipeline &curPipeline);

private:
    VkDevice device;
    VkPipelineLayout commonPipelineLayout;

    std::shared_ptr<MemoryAllocator> allocator;
    std::shared_ptr<CommandBufferManager> cmdManager;
    std::shared_ptr<Framebuffers> storageFramebuffers;

    std::shared_ptr<RasterPass> rasterPass;
    std::shared_ptr<SwapchainPass> swapchainPass;

    std::shared_ptr<RasterizedDataCollectorGeneral> collectorGeneral;
    std::shared_ptr<RasterizedDataCollectorSky> collectorSky;

    bool isCubemapOutdated;
    std::shared_ptr<RenderCubemap> renderCubemap;

    std::unique_ptr<LensFlares> lensFlares;
};

}