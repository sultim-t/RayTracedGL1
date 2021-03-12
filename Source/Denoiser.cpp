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

#include "Denoiser.h"

#include <cmath>
#include "Generated/ShaderCommonC.h"

RTGL1::Denoiser::Denoiser(
    VkDevice _device, 
    std::shared_ptr<Framebuffers> _framebuffers,
    const std::shared_ptr<const ShaderManager> &_shaderManager,
    const std::shared_ptr<const GlobalUniform> &_uniform,
    const std::shared_ptr<const ASManager> &_asManager)
:
    device(_device),
    framebuffers(std::move(_framebuffers)),
    pipelineLayout(VK_NULL_HANDLE),
    pipelineVerticesLayout(VK_NULL_HANDLE),
    merging(VK_NULL_HANDLE),
    gradientSamples(VK_NULL_HANDLE),
    gradientAtrous{},
    temporalAccumulation(VK_NULL_HANDLE),
    varianceEstimation(VK_NULL_HANDLE),
    atrous{}
{
    static_assert(sizeof(atrous) / sizeof(VkPipeline) == COMPUTE_SVGF_ATROUS_ITERATION_COUNT, "Wrong atrous pipeline count");
    static_assert(sizeof(gradientAtrous) / sizeof(VkPipeline) == COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT, "Wrong gradient atrous pipeline count");


    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        framebuffers->GetDescSetLayout(),
        _uniform->GetDescSetLayout(),
    };

    CreatePipelines(setLayouts.data(), setLayouts.size(), _shaderManager);


    setLayouts.push_back(
        _asManager->GetBuffersDescSetLayout()
    );

    CreateMergingPipeline(setLayouts.data(), setLayouts.size(), _shaderManager);
}

RTGL1::Denoiser::~Denoiser()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipelineLayout(device, pipelineVerticesLayout, nullptr);

    vkDestroyPipeline(device, merging, nullptr);
    vkDestroyPipeline(device, gradientSamples, nullptr);
    vkDestroyPipeline(device, temporalAccumulation, nullptr);
    vkDestroyPipeline(device, varianceEstimation, nullptr);
    
    for (VkPipeline p : gradientAtrous)
    {
        vkDestroyPipeline(device, p, nullptr);
    }

    for (VkPipeline p : atrous)
    {
        vkDestroyPipeline(device, p, nullptr);
    }
}

void RTGL1::Denoiser::MergeSamples(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<const GlobalUniform> &uniform,
    const std::shared_ptr<const ASManager> &asManager)
{
    typedef FramebufferImageIndex FI;

    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex),
        asManager->GetBuffersDescSet(frameIndex)
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipelineVerticesLayout,
                    0, setCount, sets,
                    0, nullptr);

    uint32_t wgGradCountX = (uint32_t)std::ceil(uniform->GetData()->renderWidth / COMPUTE_SVGF_GROUP_SIZE_X / COMPUTE_ASVGF_STRATA_SIZE);
    uint32_t wgGradCountY = (uint32_t)std::ceil(uniform->GetData()->renderHeight / COMPUTE_SVGF_GROUP_SIZE_Y / COMPUTE_ASVGF_STRATA_SIZE);

    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_MOTION);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DEPTH);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_NORMAL);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_NORMAL_GEOMETRY);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_RANDOM_SEED);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_DIRECT);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_SPECULAR);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_METALLIC_ROUGHNESS);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_SURFACE_POSITION);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_VIEW_DIRECTION);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_GRADIENT_SAMPLES);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, merging);
    vkCmdDispatch(cmd, wgGradCountX, wgGradCountY, 1);
}

void RTGL1::Denoiser::Denoise(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<const GlobalUniform> &uniform)
{
    typedef FramebufferImageIndex FI;


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


    uint32_t wgCountX = (uint32_t)std::ceil(uniform->GetData()->renderWidth / COMPUTE_SVGF_GROUP_SIZE_X);
    uint32_t wgCountY = (uint32_t)std::ceil(uniform->GetData()->renderHeight / COMPUTE_SVGF_GROUP_SIZE_Y);
    uint32_t wgGradCountX = (uint32_t)std::ceil(uniform->GetData()->renderWidth / COMPUTE_SVGF_GROUP_SIZE_X / COMPUTE_ASVGF_STRATA_SIZE);
    uint32_t wgGradCountY = (uint32_t)std::ceil(uniform->GetData()->renderHeight / COMPUTE_SVGF_GROUP_SIZE_Y / COMPUTE_ASVGF_STRATA_SIZE);


    // gradient samples
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_GRADIENT_SAMPLES);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_DIRECT);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_SPECULAR);

    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_PING_GRADIENT);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientSamples);
    vkCmdDispatch(cmd, wgGradCountX, wgGradCountY, 1);
    
    // gradient atrous

    for (uint32_t i = 0; i < COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT; i++)
    {
        if (i % 2 == 0)
        {
            framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_PING_GRADIENT);
        }
        else
        {
            framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_PONG_GRADIENT);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientAtrous[i]);
        vkCmdDispatch(cmd, wgGradCountX, wgGradCountY, 1);
    }


    // temporal accumulation
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_MOTION);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DEPTH);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_DIRECT);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_SPECULAR);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_NORMAL);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_NORMAL_GEOMETRY);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_COLOR_HISTORY);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_PING_GRADIENT);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, temporalAccumulation);
    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);


    // variance estimation
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_ACCUM_COLOR);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS);
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_ACCUM_HISTORY_LENGTH);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, varianceEstimation);
    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);


    // atrous
    framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_METALLIC_ROUGHNESS);

    for (uint32_t i = 0; i < COMPUTE_SVGF_ATROUS_ITERATION_COUNT; i++)
    {
        switch (i)
        {
            case 0:
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_SPEC_PING_COLOR);
                break;
            case 1:  
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_COLOR_HISTORY);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_SPEC_PONG_COLOR);
                break;
            case 2:  
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_SPEC_PING_COLOR);
                break;
            case 3:  
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_DIFF_PONG_COLOR_AND_VARIANCE);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_SPEC_PONG_COLOR);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_R);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_G);
                framebuffers->Barrier(cmd, frameIndex, FI::FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_B);
                break;
            default: 
                assert(0);
                return;
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, atrous[i]);
        vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
    }
}

void RTGL1::Denoiser::CreateMergingPipeline(
    VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
    const std::shared_ptr<const ShaderManager> &shaderManager)
{
    VkResult r;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;

    r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineVerticesLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Denoiser with vertices pipeline layout");

    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineVerticesLayout;
    plInfo.stage = shaderManager->GetStageInfo("CASVGFMerging");

    r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &merging);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, merging, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "ASVGF Merging pipeline");
}

void RTGL1::Denoiser::CreatePipelines(
    VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
    const std::shared_ptr<const ShaderManager> &shaderManager)
{
    VkResult r;

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;

    r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);
    
    SET_DEBUG_NAME(device, pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Denoiser pipeline layout");
    

    uint32_t atrousIteration = 0;
    
    VkSpecializationMapEntry specEntry = {};
    specEntry.constantID = 0;
    specEntry.offset = 0;
    specEntry.size = sizeof(uint32_t);

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &atrousIteration;


    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineLayout;

    {
        plInfo.stage = shaderManager->GetStageInfo("CASVGFGradientSamples");

        r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &gradientSamples);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, gradientSamples, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "ASVGF Create gradient samples pipeline");
    }

    {
        const char *debugNames[COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT] = 
        {
            "ASVGF Gradient atrous iteration #0 pipeline",
            "ASVGF Gradient atrous iteration #1 pipeline",
            "ASVGF Gradient atrous iteration #2 pipeline",
            "ASVGF Gradient atrous iteration #3 pipeline",
        };

        plInfo.stage = shaderManager->GetStageInfo("CASVGFGradientAtrous");
        plInfo.stage.pSpecializationInfo = &specInfo;

        for (uint32_t i = 0; i < COMPUTE_SVGF_ATROUS_ITERATION_COUNT; i++)
        {
            atrousIteration = i;

            r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &gradientAtrous[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, gradientAtrous[i], VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, debugNames[i]);
        }
    }

    {
        plInfo.stage = shaderManager->GetStageInfo("CSVGFTemporalAccum");

        r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &temporalAccumulation);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, temporalAccumulation, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "SVGF Temporal accumulation pipeline");
    }

    {
        plInfo.stage = shaderManager->GetStageInfo("CSVGFVarianceEstim");

        r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &varianceEstimation);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, varianceEstimation, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "SVGF Variance estimation pipeline");
    }

    {
        const char *debugNames[COMPUTE_SVGF_ATROUS_ITERATION_COUNT] = 
        {
            "SVGF Atrous iteration #0 pipeline",
            "SVGF Atrous iteration #1 pipeline",
            "SVGF Atrous iteration #2 pipeline",
            "SVGF Atrous iteration #3 pipeline",
        };

        plInfo.stage = shaderManager->GetStageInfo("CSVGFAtrous");
        plInfo.stage.pSpecializationInfo = &specInfo;

        for (uint32_t i = 0; i < COMPUTE_SVGF_ATROUS_ITERATION_COUNT; i++)
        {
            atrousIteration = i;

            r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &atrous[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, atrous[i], VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, debugNames[i]);
        }
    }
}
