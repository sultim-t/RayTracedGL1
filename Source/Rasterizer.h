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
#include "RasterizedDataCollector.h"

// This class provides rasterization functionality
class Rasterizer : public ISwapchainDependency
{
public:
    explicit Rasterizer(
        VkDevice device,
        const std::shared_ptr<MemoryAllocator> &allocator,
        const std::shared_ptr<ShaderManager> &shaderManager,
        std::shared_ptr<TextureManager> textureMgr,
        VkFormat surfaceFormat,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~Rasterizer() override;

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void Upload(const RgRasterizedGeometryUploadInfo &uploadInfo, uint32_t frameIndex);
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex);

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;

private:
    void CreateRenderPass(VkFormat surfaceFormat);
    void CreatePipelineCache();
    void CreatePipelineLayout(VkDescriptorSetLayout texturesDescLayout);
    void CreatePipeline(const std::shared_ptr<ShaderManager> &shaderManager);
    void CreateFramebuffers(uint32_t width, uint32_t height, const VkImageView *pFrameAttchs, uint32_t count);
    void DestroyFramebuffers();

    // If info's viewport is not the same as current one, new VkViewport will be set.
    void TrySetViewport(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, VkViewport &curViewport);

private:
    VkDevice device;
    std::weak_ptr<TextureManager> textureMgr;

    VkRenderPass        renderPass;
    VkPipelineLayout    pipelineLayout;
    VkPipelineCache     pipelineCache;
    VkPipeline          pipeline;

    VkRect2D  fbRenderArea;
    VkViewport fbViewport;
    std::vector<VkFramebuffer> framebuffers;

    std::shared_ptr<RasterizedDataCollector> collectors[MAX_FRAMES_IN_FLIGHT];
};