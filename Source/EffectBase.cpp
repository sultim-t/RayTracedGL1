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

#include "EffectBase.h"
#include "Generated/ShaderCommonC.h"

RTGL1::EffectBase::EffectBase(VkDevice _device): device(_device), pipelineLayout(VK_NULL_HANDLE), pipelines{}
{}

RTGL1::EffectBase::~EffectBase()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
}

RTGL1::FramebufferImageIndex RTGL1::EffectBase::Apply(VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<Framebuffers> &framebuffers, const std::shared_ptr<const GlobalUniform> &uniform,
    const std::shared_ptr<const BlueNoise> &blueNoise, uint32_t width, uint32_t height,
    FramebufferImageIndex inputFramebuf)
{
    CmdLabel label(cmd, GetShaderName());


    assert(inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING || inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PONG);
    uint32_t isSourcePing = inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING;

    
    const uint32_t wgCountX = std::max(1u, (uint32_t)std::ceil((float)width  / (float)COMPUTE_EFFECT_GROUP_SIZE_X));
    const uint32_t wgCountY = std::max(1u, (uint32_t)std::ceil((float)height / (float)COMPUTE_EFFECT_GROUP_SIZE_Y));


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex),
        blueNoise->GetDescSet(),
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[isSourcePing]);

    uint8_t pushData[128];
    uint32_t pushDataSize = 0;
    if (GetPushConstData(pushData, &pushDataSize))
    {
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushDataSize, pushData);
    }

    FramebufferImageIndex fs[] =
    {
        inputFramebuf,
    };
    framebuffers->BarrierMultiple(cmd, frameIndex, fs);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);


    return isSourcePing ? FB_IMAGE_INDEX_UPSCALED_PONG : FB_IMAGE_INDEX_UPSCALED_PING;
}

void RTGL1::EffectBase::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::EffectBase::CreatePipelines(const ShaderManager *shaderManager)
{
    for (VkPipeline t : pipelines)
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
    plInfo.stage = shaderManager->GetStageInfo(GetShaderName());
    plInfo.stage.pSpecializationInfo = &specInfo;

    for (int b = 0; b <= 1; b++)
    {
        // modify specInfo.pData
        isSourcePing = b;

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &pipelines[isSourcePing]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, pipelines[isSourcePing], VK_OBJECT_TYPE_PIPELINE, (std::string(GetShaderName()) + " from " + (isSourcePing ? "Ping" : "Pong")).c_str());
    }
}

void RTGL1::EffectBase::DestroyPipelines()
{
    for (VkPipeline &t : pipelines)
    {
        vkDestroyPipeline(device, t, nullptr);
        t = VK_NULL_HANDLE;
    }
}
