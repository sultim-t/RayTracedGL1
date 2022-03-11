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
#include "RenderResolutionHelper.h"
#include "Utils.h"


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
    upsamplePipelines{},
    applyPipelines{}
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

void RTGL1::Bloom::Prepare(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<const GlobalUniform> &uniform)
{
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

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);

    for (int i = 0; i < COMPUTE_BLOOM_STEP_COUNT; i++)
    {
        uint32_t wgCountX = Utils::GetWorkGroupCount(uniform->GetData()->renderWidth  / (float)(1 << (i + 1)), COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_X);
        uint32_t wgCountY = Utils::GetWorkGroupCount(uniform->GetData()->renderHeight / (float)(1 << (i + 1)), COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_Y);

        CmdLabel label(cmd, "Bloom downsample iteration");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipelines[i]);

        switch (i)
        {
            case 0: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_PRE_FINAL); break;
            case 1: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1); break;
            case 2: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2); break;
            case 3: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3); break;
            case 4: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4); break;
            default:assert(0);  
        }

        vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
    }


    svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);


    // start from the other side
    for (int i = COMPUTE_BLOOM_STEP_COUNT - 1; i >= 0; i--)
    {
        uint32_t wgCountX = Utils::GetWorkGroupCount(uniform->GetData()->renderWidth  / (float)(1 << i), COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_X);
        uint32_t wgCountY = Utils::GetWorkGroupCount(uniform->GetData()->renderHeight / (float)(1 << i), COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_Y);

        CmdLabel label(cmd, "Bloom upsample iteration");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upsamplePipelines[i]);

        switch (i)
        {
            case 4: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP5); break;
            case 3: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4); break;
            case 2: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3); break;
            case 1: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2); break;
            case 0: framebuffers->BarrierOne(cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1); break;
            default:assert(0);
        }

        vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
    }


    svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}

RTGL1::FramebufferImageIndex RTGL1::Bloom::Apply(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<const GlobalUniform> &uniform,
                                                 uint32_t width, uint32_t height, FramebufferImageIndex inputFramebuf)
{

    CmdLabel label(cmd, "Bloom apply");


    assert(inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING || inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PONG);
    uint32_t isSourcePing = inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING;


    const uint32_t wgCountX = Utils::GetWorkGroupCount(width, COMPUTE_BLOOM_APPLY_GROUP_SIZE_X);
    const uint32_t wgCountY = Utils::GetWorkGroupCount(height, COMPUTE_BLOOM_APPLY_GROUP_SIZE_Y);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex)
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, applyPipelines[isSourcePing]);

    FramebufferImageIndex fs[] =
    {
    inputFramebuf,
    FB_IMAGE_INDEX_BLOOM_RESULT
    };
    framebuffers->BarrierMultiple(cmd, frameIndex, fs);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);


    return isSourcePing ? FB_IMAGE_INDEX_UPSCALED_PONG : FB_IMAGE_INDEX_UPSCALED_PING;
}

void RTGL1::Bloom::OnShaderReload(const ShaderManager * shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::Bloom::CreatePipelineLayout(VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Bloom pipeline layout");
}

void RTGL1::Bloom::CreatePipelines(const ShaderManager *shaderManager)
{
    CreateStepPipelines(shaderManager);
    CreateApplyPipelines(shaderManager);
}

void RTGL1::Bloom::CreateStepPipelines(const ShaderManager *shaderManager)
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

void RTGL1::Bloom::CreateApplyPipelines(const ShaderManager *shaderManager)
{
    for (VkPipeline t : applyPipelines)
    {
        assert(t == VK_NULL_HANDLE);
    }


    uint32_t isSourcePing = 0;

    VkSpecializationMapEntry specEntry = {};
    specEntry.constantID = 0;
    specEntry.offset = 0;
    specEntry.size = sizeof(isSourcePing);

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(isSourcePing);
    specInfo.pData = &isSourcePing;


    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CBloomApply");
    plInfo.stage.pSpecializationInfo = &specInfo;

    for (int b = 0; b <= 1; b++)
    {
        // modify specInfo.pData
        isSourcePing = b;
        
        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &applyPipelines[isSourcePing]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, applyPipelines[isSourcePing], VK_OBJECT_TYPE_PIPELINE, ("Bloom apply from " + std::string(isSourcePing ? "Ping" : "Pong")).c_str());
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

    for (VkPipeline &t : applyPipelines)
    {
        vkDestroyPipeline(device, t, nullptr);
        t = VK_NULL_HANDLE;
    }
}
