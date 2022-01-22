// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "AutoBuffer.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "ShaderManager.h"
#include "TextureManager.h"

namespace RTGL1
{

class DecalManager : public IShaderDependency, public IFramebuffersDependency
{
public:
    DecalManager(VkDevice device,
                 std::shared_ptr<MemoryAllocator> &allocator,
                 const std::shared_ptr<ShaderManager> &shaderManager,
                 const std::shared_ptr<GlobalUniform> &uniform,
                 const std::shared_ptr<Framebuffers> &framebuffers,
                 const std::shared_ptr<TextureManager> &textureManager);
    ~DecalManager() override;

    DecalManager(const DecalManager &other) = delete;
    DecalManager(DecalManager &&other) noexcept = delete;
    DecalManager &operator=(const DecalManager &other) = delete;
    DecalManager &operator=(DecalManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void Upload(uint32_t frameIndex, const RgDecalUploadInfo &uploadInfo,
                const std::shared_ptr<TextureManager> &textureManager);
    void SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex,
              const std::shared_ptr<GlobalUniform> &uniform,
              const std::shared_ptr<Framebuffers> &framebuffers,
              const std::shared_ptr<TextureManager> &textureManager);

    void OnShaderReload(const ShaderManager *shaderManager) override;
    void OnFramebuffersSizeChange(uint32_t width, uint32_t height) override;

private:
    void CreateRenderPass();
    void CreateFramebuffer(uint32_t width, uint32_t height);
    void DestroyFramebuffer();
    void CreatePipelineLayout(const VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();
    void CreateDescriptors();

private:
    VkDevice device;

    std::unique_ptr<AutoBuffer> instanceBuffer;
    uint32_t decalCount;

    VkRenderPass renderPass;
    VkFramebuffer framebuffer;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkDescriptorPool descPool;
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSet descSet;
};

}