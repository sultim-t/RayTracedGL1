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

#include "Bloom.h"

#include <cmath>

#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"

RTGL1::Bloom::Bloom(
    VkDevice _device,
    std::shared_ptr<Framebuffers> _framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager,
    const std::shared_ptr<const GlobalUniform> &_uniform)
    :
    device(_device),
    framebuffers(std::move(_framebuffers)),
    pipelineLayout(VK_NULL_HANDLE),
    downsamplePipelines{},
    upsamplePipelines{}
{
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        framebuffers->GetDescSetLayout(),
        _uniform->GetDescSetLayout()
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());
    CreatePipelines(_shaderManager.get());

    static_assert(sizeof(downsamplePipelines) / sizeof(downsamplePipelines[0]) == COMPUTE_BLOOM_STEP_COUNT, "Recheck COMPUTE_BLOOM_STEP_COUNT");
    static_assert(sizeof(upsamplePipelines) / sizeof(upsamplePipelines[0]) == COMPUTE_BLOOM_STEP_COUNT, "Recheck COMPUTE_BLOOM_STEP_COUNT");
}

RTGL1::Bloom::~Bloom()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
}

void RTGL1::Bloom::Apply(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<const GlobalUniform> &uniform, bool wasNoRayTracing)
{
    typedef FramebufferImageIndex FI;


    VkMemoryBarrier2KHR memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT_KHR;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

    VkDependencyInfoKHR dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex)
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, setCount, sets,
                            0, nullptr);

    uint32_t forceIsSky = wasNoRayTracing;
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(uint32_t), &forceIsSky);

    for (int i = 0; i < COMPUTE_BLOOM_STEP_COUNT; i++)
    {
        uint32_t wgCountX = std::max(1u, (uint32_t)std::ceil(uniform->GetData()->renderWidth  / (float)((1 << (i + 1)) * COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_X)));
        uint32_t wgCountY = std::max(1u, (uint32_t)std::ceil(uniform->GetData()->renderHeight / (float)((1 << (i + 1)) * COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_Y)));

        CmdLabel label(cmd, "Bloom downsample iteration");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipelines[i]);

        switch (i)
        {
            case 0: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_PRE_FINAL); break;
            case 1: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1); break;
            case 2: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2); break;
            case 3: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3); break;
            case 4: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4); break;
        }

        vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
    }


    svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);


    // start from the other side
    for (int i = COMPUTE_BLOOM_STEP_COUNT - 1; i >= 0; i--)
    {
        uint32_t wgCountX = std::max(1u, (uint32_t)std::ceil(uniform->GetData()->renderWidth  / (float)((1 << i) * COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_X)));
        uint32_t wgCountY = std::max(1u, (uint32_t)std::ceil(uniform->GetData()->renderHeight / (float)((1 << i) * COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_Y)));

        CmdLabel label(cmd, "Bloom upsample iteration");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upsamplePipelines[i]);

        switch (i)
        {
            case 4: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP5); break;
            case 3: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4); break;
            case 2: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3); break;
            case 1: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2); break;
            case 0: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1); break;
        }

        vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
    }


    svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}

void RTGL1::Bloom::OnShaderReload(const ShaderManager * shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::Bloom::CreatePipelineLayout(VkDescriptorSetLayout * pSetLayouts, uint32_t setLayoutCount)
{
    VkPushConstantRange push = {};
    push.offset = 0;
    push.size = sizeof(uint32_t);
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;
    plLayoutInfo.pushConstantRangeCount = 1;
    plLayoutInfo.pPushConstantRanges = &push;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Bloom pipeline layout");
}

void RTGL1::Bloom::CreatePipelines(const ShaderManager * shaderManager)
{
    assert(pipelineLayout != VK_NULL_HANDLE);

    const char *dnsmplDebugNames[COMPUTE_BLOOM_STEP_COUNT] =
    {
        "Bloom downsample iteration #0 pipeline",
        "Bloom downsample iteration #1 pipeline",
        "Bloom downsample iteration #2 pipeline",
        "Bloom downsample iteration #3 pipeline",
        "Bloom downsample iteration #4 pipeline",
    };

    const char *upsmplDebugNames[COMPUTE_BLOOM_STEP_COUNT] =
    {
        "Bloom upsample iteration #0 pipeline",
        "Bloom upsample iteration #1 pipeline",
        "Bloom upsample iteration #2 pipeline",
        "Bloom upsample iteration #3 pipeline",
        "Bloom upsample iteration #4 pipeline",
    };

    uint32_t stepIndex = 0;

    VkSpecializationMapEntry specEntry = {};
    specEntry.constantID = 0;
    specEntry.offset = 0;
    specEntry.size = sizeof(uint32_t);

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &stepIndex;

    for (uint32_t i = 0; i < COMPUTE_BLOOM_STEP_COUNT; i++)
    {
        stepIndex = i;

        assert(downsamplePipelines[stepIndex] == VK_NULL_HANDLE);
        assert(upsamplePipelines[stepIndex] == VK_NULL_HANDLE);

        VkComputePipelineCreateInfo plInfo = {};
        plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        plInfo.layout = pipelineLayout;

        {
            plInfo.stage = shaderManager->GetStageInfo("CBloomDownsample");
            plInfo.stage.pSpecializationInfo = &specInfo;

            VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &downsamplePipelines[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, downsamplePipelines[i], VK_OBJECT_TYPE_PIPELINE, dnsmplDebugNames[i]);
        }

        {
            plInfo.stage = shaderManager->GetStageInfo("CBloomUpsample");
            plInfo.stage.pSpecializationInfo = &specInfo;

            VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &upsamplePipelines[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, upsamplePipelines[i], VK_OBJECT_TYPE_PIPELINE, upsmplDebugNames[i]);
        }
    }
}

void RTGL1::Bloom::DestroyPipelines()
{
    for (VkPipeline &p : downsamplePipelines)
    {
        vkDestroyPipeline(device, p, nullptr);
        p = VK_NULL_HANDLE;
    }

    for (VkPipeline &p : upsamplePipelines)
    {
        vkDestroyPipeline(device, p, nullptr);
        p = VK_NULL_HANDLE;
    }
}
