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

#include <unordered_map>

#include "Common.h"
#include "ShaderManager.h"
#include "Framebuffers.h"

namespace RTGL1
{


class RenderResolutionHelper;


class Sharpening final : public IShaderDependency
{
public:
    Sharpening(
        VkDevice device,
        const std::shared_ptr<const Framebuffers> &framebuffers,
        const std::shared_ptr<const ShaderManager> &shaderManager);
    ~Sharpening() override;

    Sharpening(const Sharpening &other) = delete;
    Sharpening(Sharpening &&other) noexcept = delete;
    Sharpening & operator=(const Sharpening &other) = delete;
    Sharpening & operator=(Sharpening &&other) noexcept = delete;

    // sharpness: 0 - none, 1 - full
    FramebufferImageIndex Apply(
        VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers,
        uint32_t width, uint32_t height, FramebufferImageIndex inputFramebuf, 
        RgRenderSharpenTechnique sharpenTechnique, float sharpenIntensity);

    void OnShaderReload(const ShaderManager *shaderManager) override;

private:
    void CreatePipelineLayout(VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();
    VkPipeline *GetPipeline(RgRenderSharpenTechnique technique, uint32_t isSourcePing);

private:
    VkDevice device;

    VkPipelineLayout pipelineLayout;
    VkPipeline simpleSharpPipelines[2];
    VkPipeline casPipelines[2];
};

}
