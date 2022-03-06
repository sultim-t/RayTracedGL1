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

static_assert(RTGL1::EFFECT_BASE_COMPUTE_GROUP_SIZE_X == COMPUTE_EFFECT_GROUP_SIZE_X, "Change group size in EffectBase.h");
static_assert(RTGL1::EFFECT_BASE_COMPUTE_GROUP_SIZE_Y == COMPUTE_EFFECT_GROUP_SIZE_Y, "Change group size in EffectBase.h");

RTGL1::EffectBase::EffectBase(VkDevice _device): device(_device), pipelineLayout(VK_NULL_HANDLE), pipelines{}
{}

RTGL1::EffectBase::~EffectBase()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
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
