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
#include "ShaderManager.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "MemoryAllocator.h"
#include "Buffer.h"

namespace RTGL1
{

class Tonemapping
{
public:
    Tonemapping(
        VkDevice device,
        std::shared_ptr<Framebuffers> framebuffers,
        const std::shared_ptr<const ShaderManager> &shaderManager,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<MemoryAllocator> &allocator);
    ~Tonemapping();

    Tonemapping(const Tonemapping &other) = delete;
    Tonemapping(Tonemapping &&other) noexcept = delete;
    Tonemapping &operator=(const Tonemapping &other) = delete;
    Tonemapping &operator=(Tonemapping &&other) noexcept = delete;

    void Tonemap(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<const GlobalUniform> &uniform);

private:
    void CreateHistogramBuffer(const std::shared_ptr<MemoryAllocator> &allocator);
    void CreateHistogramDescriptors();

    void CreatePipeline(
        VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
        const std::shared_ptr<const ShaderManager> &shaderManager);

private:
    VkDevice device;

    std::shared_ptr<Framebuffers> framebuffers;

    Buffer histogramBuffer;
    VkDescriptorSetLayout histogramDescSetLayout;
    VkDescriptorPool histogramDescPool;
    VkDescriptorSet histogramDescSet;

    VkPipelineLayout pipelineLayout;

    VkPipeline histogramPipeline;
    VkPipeline avgLuminancePipeline;
};

}