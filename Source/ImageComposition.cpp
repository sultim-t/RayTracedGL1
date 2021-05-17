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

#include "ImageComposition.h"

#include <vector>
#include <cmath>

#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"

RTGL1::ImageComposition::ImageComposition(
    VkDevice _device,
    std::shared_ptr<Framebuffers> _framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager,
    const std::shared_ptr<const GlobalUniform> &_uniform,
    const std::shared_ptr<const Tonemapping> &_tonemapping)
:
    device(_device),
    framebuffers(std::move(_framebuffers))
{
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        framebuffers->GetDescSetLayout(),
        _uniform->GetDescSetLayout(),
        _tonemapping->GetDescSetLayout()
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());
    CreatePipelines(_shaderManager.get());
}

RTGL1::ImageComposition::~ImageComposition()
{
    vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    DestroyPipelines();
}

void RTGL1::ImageComposition::Compose(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<const GlobalUniform> &uniform,
    const std::shared_ptr<const Tonemapping> &tonemapping)
{
    CmdLabel label(cmd, "Final framebuf compose");


    // sync access
    framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);


    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex),
        tonemapping->GetDescSet()
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 
                            0, setCount, sets, 
                            0, nullptr);

    // start compute shader
    uint32_t wgCountX = (uint32_t)std::ceil(uniform->GetData()->renderWidth / COMPUTE_COMPOSE_GROUP_SIZE_X);
    uint32_t wgCountY = (uint32_t)std::ceil(uniform->GetData()->renderHeight / COMPUTE_COMPOSE_GROUP_SIZE_Y);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
}

void RTGL1::ImageComposition::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::ImageComposition::CreatePipelineLayout(VkDescriptorSetLayout*pSetLayouts, uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;
    
    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &computePipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, computePipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Composition pipeline layout");
}

void RTGL1::ImageComposition::CreatePipelines(const ShaderManager *shaderManager)
{
    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = computePipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CComposition");

    VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &computePipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, computePipeline, VK_OBJECT_TYPE_PIPELINE, "Composition pipeline");
}

void RTGL1::ImageComposition::DestroyPipelines()
{
    vkDestroyPipeline(device, computePipeline, nullptr);
    computePipeline = VK_NULL_HANDLE;
}
