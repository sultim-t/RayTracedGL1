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
#include "Framebuffers.h"
#include "ShaderManager.h"

namespace RTGL1
{

// Copy depth data from a storage buffer to a depth buffer.
class DepthCopying
{
public:
    DepthCopying(VkDevice device,
                 VkFormat depthFormat,
                 const std::shared_ptr<ShaderManager> &shaderManager,
                 const std::shared_ptr<Framebuffers> &storageFramebuffers);
    ~DepthCopying();

    DepthCopying(const DepthCopying &other) = delete;
    DepthCopying(DepthCopying &&other) noexcept = delete;
    DepthCopying &operator=(const DepthCopying &other) = delete;
    DepthCopying &operator=(DepthCopying &&other) noexcept = delete;

    // Copy storage buffer data to depth buffer.
    // If justClear is true, target depth buffer will be only cleared.
    void Process(VkCommandBuffer cmd, uint32_t frameIndex,
                 const std::shared_ptr<Framebuffers> &storageFramebuffers,
                 uint32_t width, uint32_t height,
                 bool justClear);

    void CreateFramebuffers(VkImageView pDepthAttchViews[MAX_FRAMES_IN_FLIGHT], uint32_t width, uint32_t height);
    void DestroyFramebuffers();

    void OnShaderReload(const ShaderManager *shaderManager);

private:
    void CreateRenderPass(VkFormat depthFormat);
    void CreatePipelineLayout(VkDescriptorSetLayout fbSetLayout);
    void CreatePipeline(const ShaderManager *shaderManager);

private:
    VkDevice device;

    VkRenderPass renderPass;
    VkFramebuffer framebuffers[MAX_FRAMES_IN_FLIGHT];

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

};

}
