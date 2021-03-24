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
#include "RTGL1/RTGL1.h"
#include "ShaderManager.h"
#include "ISwapchainDependency.h"
#include "IFramebuffersDependency.h"
#include "RasterizedDataCollector.h"
#include "Framebuffers.h"

namespace RTGL1
{

// This class provides rasterization functionality
class Rasterizer : public ISwapchainDependency, public IShaderDependency, public IFramebuffersDependency
{
public:
    explicit Rasterizer(
        VkDevice device,
        const std::shared_ptr<MemoryAllocator> &allocator,
        const std::shared_ptr<ShaderManager> &shaderManager,
        std::shared_ptr<TextureManager> textureMgr,
        std::shared_ptr<Framebuffers> storageFramebuffers,
        VkFormat surfaceFormat,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~Rasterizer() override;

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void Upload(uint32_t frameIndex, 
                const RgRasterizedGeometryUploadInfo &uploadInfo, 
                const float *viewProjection, const RgViewport *viewport);
    void DrawToFinalImage(VkCommandBuffer cmd, uint32_t frameIndex, float *view, float *proj);
    void DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainIndex, float *view, float *proj);

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;
    
    void OnShaderReload(const ShaderManager *shaderManager) override;
    void OnFramebuffersSizeChange(uint32_t width, uint32_t height) override;

private:
    struct RasterAreaState
    {
        VkRect2D renderArea = {};
        VkViewport viewport = {};
    };

private:
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex, 
              const std::vector<RasterizedDataCollector::DrawInfo> &drawInfos,
              VkRenderPass renderPass, VkPipeline pipeline,
              VkFramebuffer framebuffer, const RasterAreaState &raState, float *defaultViewProj);

    void CreateRasterRenderPass(VkFormat finalImageFormat, VkFormat depthImageFormat);
    void CreateSwapchainRenderPass(VkFormat surfaceFormat);

    void CreatePipelineCache();
    void CreatePipelineLayout(VkDescriptorSetLayout *pSetLayouts, uint32_t count);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();

    void CreateRenderFramebuffers(uint32_t renderWidth, uint32_t renderHeight);
    void DestroyRenderFramebuffers();

    void CreateSwapchainFramebuffers(uint32_t swapchainWidth, uint32_t swapchainHeight,
                                     const VkImageView *pSwapchainAttchs, uint32_t swapchainAttchCount);
    void DestroySwapchainFramebuffers();

    // If info's viewport is not the same as current one, new VkViewport will be set.
    void SetViewportIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info,  
                          const VkViewport &defaultViewport, VkViewport &curViewport) const;

private:
    VkDevice device;
    std::weak_ptr<TextureManager> textureMgr;
    std::shared_ptr<Framebuffers> storageFramebuffers;

    VkRenderPass        rasterRenderPass;
    VkRenderPass        swapchainRenderPass;

    VkPipelineLayout    pipelineLayout;

    // TODO: separate class for handling different pipeline settings
    VkPipeline          rasterPipeline;
    VkPipeline          swapchainPipeline;

    RasterAreaState rasterFramebufferState;
    VkFramebuffer rasterFramebuffers[MAX_FRAMES_IN_FLIGHT];

    RasterAreaState swapchainFramebufferState;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    std::shared_ptr<RasterizedDataCollector> collectors[MAX_FRAMES_IN_FLIGHT];
};

}