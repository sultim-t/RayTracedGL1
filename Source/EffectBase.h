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

#include "CmdLabel.h"
#include "Common.h"
#include "ShaderManager.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "BlueNoise.h"

namespace RTGL1
{

class EffectBase : public IShaderDependency
{
public:
    explicit EffectBase(VkDevice _device);

    ~EffectBase() override;

    EffectBase(const EffectBase &other) = delete;
    EffectBase(EffectBase &&other) noexcept = delete;
    EffectBase &operator=(const EffectBase &other) = delete;
    EffectBase &operator=(EffectBase &&other) noexcept = delete;

    FramebufferImageIndex Apply(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<Framebuffers> &framebuffers, const std::shared_ptr<const GlobalUniform> &uniform, const std::shared_ptr<const BlueNoise> &blueNoise,
        uint32_t width, uint32_t height, FramebufferImageIndex inputFramebuf);

    void OnShaderReload(const ShaderManager *shaderManager) override;

protected:
    // Call this function in a child class constructor
    template <typename PUSH_CONST_T = std::nullptr_t, int DESC_SET_COUNT>
    void InitBase(
        const std::shared_ptr<const Framebuffers>  &framebuffers,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<const BlueNoise>     &blueNoise,
        const std::shared_ptr<const ShaderManager> &shaderManager, 
        const VkDescriptorSetLayout(&setLayouts)[DESC_SET_COUNT],
        const PUSH_CONST_T &push);

    virtual const char *GetShaderName() = 0;

    virtual bool GetPushConstData(uint8_t(&pData)[128], uint32_t *pDataSize) const { return false; }

private:
    template <typename PUSH_CONST_T, int DESC_SET_COUNT>
    void CreatePipelineLayout(const VkDescriptorSetLayout (&setLayouts)[DESC_SET_COUNT]);
    void CreatePipelines(const ShaderManager *shaderManager);
    void DestroyPipelines();

private:
    VkDevice device;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipelines[2];
};


template<typename PUSH_CONST_T, int DESC_SET_COUNT>
void EffectBase::InitBase(
    const std::shared_ptr<const Framebuffers>  &framebuffers,
    const std::shared_ptr<const GlobalUniform> &uniform, 
    const std::shared_ptr<const BlueNoise>     &blueNoise,
    const std::shared_ptr<const ShaderManager> &shaderManager,

    const VkDescriptorSetLayout(&setLayouts)[DESC_SET_COUNT],
    const PUSH_CONST_T&)
{
    static_assert(sizeof(PUSH_CONST_T) <= 128, "Push constant must have size <= 128");

    CreatePipelineLayout<PUSH_CONST_T, DESC_SET_COUNT>(setLayouts);
    CreatePipelines(shaderManager.get());
}


template<typename PUSH_CONST_T, int DESC_SET_COUNT>
void EffectBase::CreatePipelineLayout(const VkDescriptorSetLayout(&setLayouts)[DESC_SET_COUNT])
{
    VkPushConstantRange push = {};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.offset = 0;
    push.size = sizeof(PUSH_CONST_T);

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = DESC_SET_COUNT;
    plLayoutInfo.pSetLayouts = setLayouts;
    plLayoutInfo.pushConstantRangeCount = std::is_same_v<std::nullptr_t, PUSH_CONST_T> ? 0 : 1;
    plLayoutInfo.pPushConstantRanges = &push;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Bloom pipeline layout");
}

}
