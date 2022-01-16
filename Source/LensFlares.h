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
#include "RasterizerPipelines.h"

namespace RTGL1
{

class LensFlares : public IShaderDependency
{
public:
    LensFlares(VkDevice device, 
               std::shared_ptr<MemoryAllocator> &allocator, 
               const std::shared_ptr<ShaderManager> &shaderManager,
               VkRenderPass renderPass,
               std::shared_ptr<GlobalUniform> uniform,
               std::shared_ptr<Framebuffers> framebuffers,
               std::shared_ptr<TextureManager> textureManager,
               const RgInstanceCreateInfo &instanceInfo);
    ~LensFlares() override;

    LensFlares(const LensFlares &other) = delete;
    LensFlares(LensFlares &&other) noexcept = delete;
    LensFlares &operator=(const LensFlares &other) = delete;
    LensFlares &operator=(LensFlares &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void Upload(uint32_t frameIndex, const RgLensFlareUploadInfo &uploadInfo);
    void SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void Cull(VkCommandBuffer cmd, uint32_t frameIndex);
    void SyncForDraw(VkCommandBuffer cmd, uint32_t frameIndex);
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex);

    uint32_t GetCullingInputCount() const;

    void OnShaderReload(const ShaderManager *shaderManager) override;

private:
    void CreateDescriptors();
    void CreatePipelineLayouts(VkDescriptorSetLayout uniform,
                               VkDescriptorSetLayout textures, 
                               VkDescriptorSetLayout lensFlaresCull,
                               VkDescriptorSetLayout framebufs);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();

private:
    VkDevice device;
    std::shared_ptr<GlobalUniform> uniform;
    std::shared_ptr<Framebuffers> framebuffers;
    std::shared_ptr<TextureManager> textureManager;


    std::unique_ptr<AutoBuffer> cullingInput;
    Buffer indirectDrawCommands;

    std::unique_ptr<AutoBuffer> vertexBuffer;
    std::unique_ptr<AutoBuffer> indexBuffer;
    std::unique_ptr<AutoBuffer> instanceBuffer;

    uint32_t cullingInputCount;
    uint32_t vertexCount;
    uint32_t indexCount;


    VkPipelineLayout vertFragPipelineLayout;
    VkPipelineLayout cullPipelineLayout;

    std::unique_ptr<RasterizerPipelines> pipelines;
    VkPipeline cullPipeline;


    VkDescriptorPool cullDescPool;
    VkDescriptorSet cullDescSets[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSetLayout cullDescSetLayout;
};

}