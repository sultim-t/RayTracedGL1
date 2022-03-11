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

#include "SuperResolution.h"

#include "CmdLabel.h"
#include "RenderResolutionHelper.h"

#define A_CPU
#include "Utils.h"
#include "Shaders/FSR/ffx_a.h"
#include "Shaders/FSR/ffx_fsr1.h"

struct FsrPush
{
    uint32_t con0[4];
    uint32_t con1[4];
    uint32_t con2[4];
    uint32_t con3[4];
};

RTGL1::SuperResolution::SuperResolution(
    VkDevice _device,
    const std::shared_ptr<const Framebuffers> &_framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager)
:
    device(_device),
    pipelineLayout(VK_NULL_HANDLE),
    pipelineEasu(VK_NULL_HANDLE),
    pipelineRcas(VK_NULL_HANDLE)
{
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        _framebuffers->GetDescSetLayout(),
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());
    CreatePipelines(_shaderManager.get());
}

RTGL1::SuperResolution::~SuperResolution()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
}

RTGL1::FramebufferImageIndex RTGL1::SuperResolution::Apply(
    VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers,
    const RenderResolutionHelper &renderResolution)
{
    CmdLabel label(cmd, "FSR Upscale");


    const uint32_t threadGroupWorkRegionDim = 16;
    uint32_t dispatchX = Utils::GetWorkGroupCount(renderResolution.UpscaledWidth(), threadGroupWorkRegionDim);
    uint32_t dispatchY = Utils::GetWorkGroupCount(renderResolution.UpscaledHeight(), threadGroupWorkRegionDim);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);


    // EASU
    {
        FsrPush easuCon;
        FsrEasuCon(
            easuCon.con0, easuCon.con1, easuCon.con2, easuCon.con3,
            (AF1)renderResolution.Width(),         (AF1)renderResolution.Height(),          // viewport size
            (AF1)renderResolution.Width(),         (AF1)renderResolution.Height(),          // image resource size
            (AF1)renderResolution.UpscaledWidth(), (AF1)renderResolution.UpscaledHeight()   // upscaled size
        );

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(easuCon), &easuCon);

        framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineEasu);
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);
    }


    // RCAS
    {
        FsrPush rcasCon;
        FsrRcasCon(
            rcasCon.con0,
            (AF1)renderResolution.GetAmdFsrSharpness()
        );

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(rcasCon), &rcasCon);

        framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PING);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineRcas);
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);
    }

    return FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PONG;
}

void RTGL1::SuperResolution::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::SuperResolution::CreatePipelineLayout(VkDescriptorSetLayout*pSetLayouts, uint32_t setLayoutCount)
{
    VkPushConstantRange push = {};
    push.offset = 0;
    push.size = sizeof(FsrPush);
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;
    plLayoutInfo.pushConstantRangeCount = 1;
    plLayoutInfo.pPushConstantRanges = &push;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "FSR pipeline layout");
}

void RTGL1::SuperResolution::CreatePipelines(const ShaderManager *shaderManager)
{
    assert(pipelineLayout != VK_NULL_HANDLE);
    assert(pipelineEasu == VK_NULL_HANDLE);
    assert(pipelineRcas == VK_NULL_HANDLE);

    {
        VkComputePipelineCreateInfo plInfo = {};
        plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        plInfo.layout = pipelineLayout;
        plInfo.stage = shaderManager->GetStageInfo("CFsrEasu");

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &pipelineEasu);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, pipelineEasu, VK_OBJECT_TYPE_PIPELINE, "FSR EASU pipeline");
    }

    {
        VkComputePipelineCreateInfo plInfo = {};
        plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        plInfo.layout = pipelineLayout;
        plInfo.stage = shaderManager->GetStageInfo("CFsrRcas");

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &pipelineRcas);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, pipelineRcas, VK_OBJECT_TYPE_PIPELINE, "FSR RCAS pipeline");
    }

}

void RTGL1::SuperResolution::DestroyPipelines()
{
    assert(pipelineEasu != VK_NULL_HANDLE);
    assert(pipelineRcas != VK_NULL_HANDLE);

    vkDestroyPipeline(device, pipelineEasu, nullptr);
    pipelineEasu = VK_NULL_HANDLE;

    vkDestroyPipeline(device, pipelineRcas, nullptr);
    pipelineRcas = VK_NULL_HANDLE;
}
