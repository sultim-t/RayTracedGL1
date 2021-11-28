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

namespace RTGL1
{


class RenderResolutionHelper;


class SuperResolution final : public IShaderDependency
{
public:
    SuperResolution(
        VkDevice device,
        const std::shared_ptr<const Framebuffers> &framebuffers,
        const std::shared_ptr<const ShaderManager> &shaderManager);
    ~SuperResolution() override;

    SuperResolution(const SuperResolution &other) = delete;
    SuperResolution(SuperResolution &&other) noexcept = delete;
    SuperResolution & operator=(const SuperResolution &other) = delete;
    SuperResolution & operator=(SuperResolution &&other) noexcept = delete;

    FramebufferImageIndex Apply(
        VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers,
        const RenderResolutionHelper &renderResolution);

    void OnShaderReload(const ShaderManager *shaderManager) override;

private:
    void CreatePipelineLayout(VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();

private:
    VkDevice device;

    VkPipelineLayout pipelineLayout;

    VkPipeline pipelineEasu;
    VkPipeline pipelineRcas;
};

}
