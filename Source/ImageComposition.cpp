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

RTGL1::ImageComposition::ImageComposition(
    VkDevice _device,
    std::shared_ptr<Framebuffers> _framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager,
    const std::shared_ptr<const GlobalUniform> &uniform)
:
    device(_device),
    framebuffers(std::move(_framebuffers))
{


    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        framebuffers->GetDescSetLayout(),
        uniform->GetDescSetLayout()
    };

    CreatePipeline(setLayouts.data(), setLayouts.size(), _shaderManager);
}

RTGL1::ImageComposition::~ImageComposition()
{
    vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
}

void RTGL1::ImageComposition::Compose(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<const GlobalUniform> &uniform)
{
    // sync access
    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_ALBEDO);
    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_DEPTH);
    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_METALLIC_ROUGHNESS);
    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_LIGHT_DIRECT_DIFFUSE);
    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_LIGHT_DIRECT_SPECULAR);


    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex)
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 
                            0, setCount, sets, 
                            0, nullptr);

    // start compute shader
    uint32_t wgCountX = std::ceil(uniform->GetData()->renderWidth / COMPUTE_COMPOSE_WORKGROUP_SIZE_X);
    uint32_t wgCountY = std::ceil(uniform->GetData()->renderHeight / COMPUTE_COMPOSE_WORKGROUP_SIZE_Y);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
}

void RTGL1::ImageComposition::CreatePipeline(
    VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
    const std::shared_ptr<const ShaderManager> &shaderManager)
{
    VkResult r;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;
    
    r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &computePipelineLayout);
    VK_CHECKERROR(r);

    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = computePipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CComposition");

    r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &computePipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, computePipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Composition pipeline layout");
    SET_DEBUG_NAME(device, computePipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Composition pipeline");
}
