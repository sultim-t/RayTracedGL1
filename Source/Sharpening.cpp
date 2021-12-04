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
    pipelineLayout(VK_NULL_HANDLE),
    pipelineFromFinal(VK_NULL_HANDLE),
    pipelineFromUpscaled(VK_NULL_HANDLE)
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

        framebuffers->BarrierOne(cmd, frameIndex, wasUpscalePass ? SOURCE_UPSCALED_FRAMEBUF :
                                                                   FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, wasUpscalePass ? pipelineFromUpscaled : pipelineFromFinal);
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
    assert(pipelineFromFinal == VK_NULL_HANDLE);
    assert(pipelineFromUpscaled == VK_NULL_HANDLE);

    FramebufferImageIndex sourceFramebufIndex;

    VkSpecializationMapEntry entry = {};
    entry.constantID = 0;
    entry.offset = 0;
    entry.size = sizeof(uint32_t);

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &entry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &sourceFramebufIndex;

    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CCas");
    plInfo.stage.pSpecializationInfo = &specInfo;

    {
        sourceFramebufIndex = FB_IMAGE_INDEX_FINAL;

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &pipelineFromFinal);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, pipelineFromFinal, VK_OBJECT_TYPE_PIPELINE, "CAS pipeline");
    }

    {
        sourceFramebufIndex = SOURCE_UPSCALED_FRAMEBUF;

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &pipelineFromUpscaled);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, pipelineFromUpscaled, VK_OBJECT_TYPE_PIPELINE, "CAS after upscale pipeline");
    }
}

void RTGL1::Sharpening::DestroyPipelines()
{
    assert(pipelineFromFinal != VK_NULL_HANDLE);
    assert(pipelineFromUpscaled != VK_NULL_HANDLE);

    vkDestroyPipeline(device, pipelineFromFinal, nullptr);
    pipelineFromFinal = VK_NULL_HANDLE;

    vkDestroyPipeline(device, pipelineFromUpscaled, nullptr);
    pipelineFromUpscaled = VK_NULL_HANDLE;
}
