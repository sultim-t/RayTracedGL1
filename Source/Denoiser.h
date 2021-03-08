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

#include "ShaderManager.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "ASManager.h"

namespace RTGL1
{

class Denoiser
{
public:
    Denoiser(
        VkDevice device,
        std::shared_ptr<Framebuffers> framebuffers,
        const std::shared_ptr<const ShaderManager> &shaderManager,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<const ASManager> &asManager);
    ~Denoiser();

    Denoiser(const Denoiser &other) = delete;
    Denoiser(Denoiser &&other) noexcept = delete;
    Denoiser & operator=(const Denoiser &other) = delete;
    Denoiser & operator=(Denoiser &&other) noexcept = delete;

    void MergeSamples(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<const ASManager> &asManager);

    void Denoise(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<const GlobalUniform> &uniform);

private:
    void CreateMergingPipeline(
        VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
        const std::shared_ptr<const ShaderManager> &shaderManager);

    void CreatePipelines(
        VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
        const std::shared_ptr<const ShaderManager> &shaderManager);

private:
    VkDevice device;

    std::shared_ptr<Framebuffers> framebuffers;

    VkPipelineLayout pipelineLayout;
    VkPipelineLayout pipelineVerticesLayout;

    VkPipeline merging;
    VkPipeline gradientSamples;
    VkPipeline gradientAtrous[4];

    VkPipeline temporalAccumulation;
    VkPipeline varianceEstimation;
    VkPipeline atrous[4];
};

}
