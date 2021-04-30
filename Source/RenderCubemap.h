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

#pragma once

#include "Common.h"
#include "GlobalUniform.h"
#include "MemoryAllocator.h"
#include "RasterizedDataCollector.h"
#include "RasterizerPipelines.h"
#include "TextureManager.h"

namespace RTGL1
{

class RenderCubemap : public IShaderDependency
{
public:
    RenderCubemap(VkDevice device,
                  const std::shared_ptr<MemoryAllocator> &allocator,
                  const std::shared_ptr<ShaderManager> &shaderManager,
                  const std::shared_ptr<TextureManager> &textureManager,
                  const std::shared_ptr<GlobalUniform> &uniform,
                  const std::shared_ptr<SamplerManager> &samplerManager,
                  const std::shared_ptr<CommandBufferManager> &cmdManager,
                  const RgInstanceCreateInfo &instanceInfo);
    ~RenderCubemap() override;

    RenderCubemap(const RenderCubemap &other) = delete;
    RenderCubemap(RenderCubemap &&other) noexcept = delete;
    RenderCubemap &operator=(const RenderCubemap &other) = delete;
    RenderCubemap &operator=(RenderCubemap &&other) noexcept = delete;

    // Draw to a cubemap
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex,
              const std::shared_ptr<RasterizedDataCollectorSky> &skyDataCollector,
              const std::shared_ptr<TextureManager> &textureManager,
              const std::shared_ptr<GlobalUniform> &uniform);

    VkDescriptorSetLayout GetDescSetLayout() const;
    VkDescriptorSet GetDescSet() const;

    void OnShaderReload(const ShaderManager *shaderManager) override;
    

private:
    struct Attachment
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
    };

private:
    void CreatePipelineLayout(VkDescriptorSetLayout texturesSetLayout, VkDescriptorSetLayout uniformSetLayout);
    void CreateRenderPass();
    void InitPipelines(const std::shared_ptr<ShaderManager> &shaderManager, uint32_t sideSize, bool applyVertexColorGamma);
    void CreateAttch(const std::shared_ptr<MemoryAllocator> &allocator, VkCommandBuffer cmd, uint32_t sideSize, Attachment &result, bool isDepth);
    void CreateFramebuffer(uint32_t sideSize);
    void CreateDescriptors(const std::shared_ptr<SamplerManager> &samplerManager);

    void BindPipelineIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, VkPipeline &curPipeline);

private:
    VkDevice device;

    VkPipelineLayout pipelineLayout;
    std::shared_ptr<RasterizerPipelines> pipelines;

    VkRenderPass multiviewRenderPass;

    Attachment cubemap;
    Attachment cubemapDepth;

    VkFramebuffer cubemapFramebuffer;

    uint32_t cubemapSize;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSet;
};

}
