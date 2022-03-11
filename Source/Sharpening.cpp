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

#include "Sharpening.h"

#include "CmdLabel.h"
#include "RenderResolutionHelper.h"


#define A_CPU
#include "Utils.h"
#include "Shaders/CAS/ffx_a.h"
#include "Shaders/CAS/ffx_cas.h"

struct CasPush
{
    uint32_t con0[4];
    uint32_t con1[4];
};


RTGL1::Sharpening::Sharpening(
    VkDevice _device,
    const std::shared_ptr<const Framebuffers> &_framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager)
:
    device(_device),
    pipelineLayout(VK_NULL_HANDLE),
    simpleSharpPipelines{},
    casPipelines{}
{
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        _framebuffers->GetDescSetLayout(),
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());
    CreatePipelines(_shaderManager.get());
}

RTGL1::Sharpening::~Sharpening()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
}

RTGL1::FramebufferImageIndex RTGL1::Sharpening::Apply(
    VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers,
    uint32_t width, uint32_t height, FramebufferImageIndex inputFramebuf, 
    RgRenderSharpenTechnique sharpenTechnique, float sharpenIntensity)
{
    if (sharpenTechnique == RG_RENDER_SHARPEN_TECHNIQUE_NONE)
    {
        return inputFramebuf;
    }

    CmdLabel label(cmd, "Sharpening");


    assert(inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING || inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PONG);
    uint32_t isSourcePing = inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING;


    const uint32_t threadGroupWorkRegionDim = 16;
    uint32_t dispatchX = Utils::GetWorkGroupCount(width, threadGroupWorkRegionDim);
    uint32_t dispatchY = Utils::GetWorkGroupCount(height, threadGroupWorkRegionDim);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);

    {
        CasPush casPush;
        CasSetup(casPush.con0, casPush.con1,
                 sharpenIntensity,              // sharpness tuning knob (0.0 to 1.0).
                 width, height,                 // input size.
                 width, height);                // output size.

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(casPush), &casPush);

        framebuffers->BarrierOne(cmd, frameIndex, inputFramebuf);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *GetPipeline(sharpenTechnique, isSourcePing));
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);
    }

    return isSourcePing ? FB_IMAGE_INDEX_UPSCALED_PONG : FB_IMAGE_INDEX_UPSCALED_PING;
}

void RTGL1::Sharpening::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::Sharpening::CreatePipelineLayout(VkDescriptorSetLayout*pSetLayouts, uint32_t setLayoutCount)
{
    VkPushConstantRange push = {};
    push.offset = 0;
    push.size = sizeof(CasPush);
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;
    plLayoutInfo.pushConstantRangeCount = 1;
    plLayoutInfo.pPushConstantRanges = &push;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "CAS pipeline layout");
}

void RTGL1::Sharpening::CreatePipelines(const ShaderManager *shaderManager)
{
    assert(pipelineLayout != VK_NULL_HANDLE);

    struct SpecData
    {
        uint32_t isSourcePing;
        uint32_t useSimpleSharp;
    } data;

    VkSpecializationMapEntry entries[2] = {};

    entries[0].constantID = 0;
    entries[0].offset = offsetof(SpecData, isSourcePing);
    entries[0].size = sizeof(data.isSourcePing);

    entries[1].constantID = 1;
    entries[1].offset = offsetof(SpecData, useSimpleSharp);
    entries[1].size = sizeof(data.useSimpleSharp);

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = std::size(entries);
    specInfo.pMapEntries = entries;
    specInfo.dataSize = sizeof(data);
    specInfo.pData = &data;

    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CCas");
    plInfo.stage.pSpecializationInfo = &specInfo;

    RgRenderSharpenTechnique ts[] =
    {
        RG_RENDER_SHARPEN_TECHNIQUE_NAIVE,
        RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS
    };

    for (auto t : ts)
    {
        for (uint32_t b = 0; b <= 1; b++)
        {
            assert(*GetPipeline(t, b) == VK_NULL_HANDLE);

            // modify specialization data
            data.isSourcePing = b;
            data.useSimpleSharp = t == RG_RENDER_SHARPEN_TECHNIQUE_NAIVE;

            VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, GetPipeline(t, b));
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, *GetPipeline(t, b), VK_OBJECT_TYPE_PIPELINE, data.useSimpleSharp ? "Simple sharpening" : "CAS");
        }
    }
}

void RTGL1::Sharpening::DestroyPipelines()
{
    for (auto &t : simpleSharpPipelines)
    {
        assert(t != VK_NULL_HANDLE);

        vkDestroyPipeline(device, t, nullptr);
        t = VK_NULL_HANDLE;
    }
    for (auto &t : casPipelines)
    {
        assert(t != VK_NULL_HANDLE);

        vkDestroyPipeline(device, t, nullptr);
        t = VK_NULL_HANDLE;
    }
}

VkPipeline *RTGL1::Sharpening::GetPipeline(RgRenderSharpenTechnique technique, uint32_t isSourcePing)
{
    switch (technique)
    {
        case RG_RENDER_SHARPEN_TECHNIQUE_NAIVE:   return &simpleSharpPipelines[isSourcePing];
        case RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS: return &casPipelines[isSourcePing];
        default: assert(0); return nullptr;
    }
}
