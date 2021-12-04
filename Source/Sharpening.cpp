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
#include "Shaders/CAS/ffx_a.h"
#include "Shaders/CAS/ffx_cas.h"

struct CasPush
{
    uint32_t con0[4];
    uint32_t con1[4];
};


// modify shader sources if this var is changed
#define SOURCE_UPSCALED_FRAMEBUF FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PONG


RTGL1::Sharpening::Sharpening(
    VkDevice _device,
    const std::shared_ptr<const Framebuffers> &_framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager)
:
    device(_device),
    pipelineLayout(VK_NULL_HANDLE)
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
    const RenderResolutionHelper &renderResolution, FramebufferImageIndex inputImage)
{
    assert(renderResolution.IsSharpeningEnabled());

    assert(inputImage == SOURCE_UPSCALED_FRAMEBUF || inputImage == FB_IMAGE_INDEX_FINAL);
    const bool wasUpscalePass = inputImage != FB_IMAGE_INDEX_FINAL;


    CmdLabel label(cmd, "Sharpening");

    
    const uint32_t width  = wasUpscalePass ? renderResolution.UpscaledWidth()  : renderResolution.Width();
    const uint32_t height = wasUpscalePass ? renderResolution.UpscaledHeight() : renderResolution.Height();


    const int threadGroupWorkRegionDim = 16;
    int dispatchX = ((int)width  + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
    int dispatchY = ((int)height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;


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
                 renderResolution.GetSharpeningIntensity(), // sharpness tuning knob (0.0 to 1.0).
                 width, height,                             // input size.
                 width, height);                            // output size.

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(casPush), &casPush);

        framebuffers->BarrierOne(cmd, frameIndex, inputImage);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *GetPipeline(renderResolution.GetSharpeningTechnique(), inputImage));
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);
    }

    // the result is always saved to this framebuf
    return FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PING;
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
        FramebufferImageIndex sourceFramebufIndex;
        uint32_t useSimpleSharp;
    } data;

    VkSpecializationMapEntry entries[2] = {};

    entries[0].constantID = 0;
    entries[0].offset = offsetof(SpecData, sourceFramebufIndex);
    entries[0].size = sizeof(data.sourceFramebufIndex);

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
    
    FramebufferImageIndex sourceFramebufIndices[] =
    {
        SOURCE_UPSCALED_FRAMEBUF,
        FB_IMAGE_INDEX_FINAL
    };

    for (uint32_t useSimpleSharp = 0; useSimpleSharp <= 1; useSimpleSharp++)
    {
        for (FramebufferImageIndex sourceFramebufIndex : sourceFramebufIndices)
        {
            assert(*GetPipeline(useSimpleSharp, sourceFramebufIndex) == VK_NULL_HANDLE);

            // modify specialization data
            data.useSimpleSharp = useSimpleSharp;
            data.sourceFramebufIndex = sourceFramebufIndex;

            VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, GetPipeline(useSimpleSharp, sourceFramebufIndex));
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, *GetPipeline(useSimpleSharp, sourceFramebufIndex), VK_OBJECT_TYPE_PIPELINE, useSimpleSharp ? "Simple sharpening" : "CAS");
        }
    }
}

void RTGL1::Sharpening::DestroyPipelines()
{
    std::unordered_map<FramebufferImageIndex, VkPipeline> *pPipelineMaps[] =
    {
        &simpleSharpPipelines,
        &casPipelines
    };

    for (auto *m : pPipelineMaps)
    {
        for (auto &t : *m)
        {
            assert(t.second != VK_NULL_HANDLE);

            vkDestroyPipeline(device, t.second, nullptr);
            t.second = VK_NULL_HANDLE;
        }
    }
}

VkPipeline *RTGL1::Sharpening::GetPipeline(RgRenderSharpenTechnique technique, FramebufferImageIndex inputImage)
{
    switch (technique)
    {
        case RG_RENDER_SHARPEN_TECHNIQUE_NAIVE:   return GetPipeline(true,  inputImage);
        case RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS: return GetPipeline(false, inputImage);
        default: assert(0); return nullptr;
    }
}

VkPipeline *RTGL1::Sharpening::GetPipeline(bool useSimpleSharp, FramebufferImageIndex inputImage)
{
    auto &map = useSimpleSharp ? simpleSharpPipelines : casPipelines;
    return &map[inputImage];
}
