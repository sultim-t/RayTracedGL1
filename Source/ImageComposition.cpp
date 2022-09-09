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

#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"
#include "Utils.h"

namespace
{
    constexpr auto LPM_BUFFER_SIZE = sizeof(uint32_t) * 24 * 4;
}

RTGL1::ImageComposition::ImageComposition(
    VkDevice                                      _device,
    std::shared_ptr< MemoryAllocator >            _allocator,
    std::shared_ptr< Framebuffers >               _framebuffers,
    const std::shared_ptr< const ShaderManager >& _shaderManager,
    const std::shared_ptr< const GlobalUniform >& _uniform,
    const std::shared_ptr< const Tonemapping >&   _tonemapping,
    const Volumetric*                             _volumetric )
    : device( _device )
    , framebuffers( std::move( _framebuffers ) )
    , lpmParamsInited( false )
    , composePipelineLayout( VK_NULL_HANDLE )
    , checkerboardPipelineLayout( VK_NULL_HANDLE )
    , composePipeline( VK_NULL_HANDLE )
    , checkerboardPipeline( VK_NULL_HANDLE )
    , descLayout( VK_NULL_HANDLE )
    , descPool( VK_NULL_HANDLE )
    , descSet( VK_NULL_HANDLE )
{
    lpmParams = std::make_unique<AutoBuffer>(std::move(_allocator));
    lpmParams->Create(LPM_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "LPM Params", 1);

    CreateDescriptors();

    {
        VkDescriptorSetLayout setLayouts[] = {
            framebuffers->GetDescSetLayout(),
            _uniform->GetDescSetLayout(),
            _tonemapping->GetDescSetLayout(),
            descLayout,
            _volumetric->GetDescSetLayout(),
        };

        composePipelineLayout = CreatePipelineLayout(device,
            setLayouts, std::size(setLayouts),
            "Composition pipeline layout");
    }
    {
        VkDescriptorSetLayout setLayouts[] =
        {
            framebuffers->GetDescSetLayout(),
            _uniform->GetDescSetLayout(),
        };

        checkerboardPipelineLayout = CreatePipelineLayout(device,
            setLayouts, std::size(setLayouts),
            "Checkerboard pipeline layout");
    }

    CreatePipelines(_shaderManager.get());
}

RTGL1::ImageComposition::~ImageComposition()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
    vkDestroyPipelineLayout(device, composePipelineLayout, nullptr);
    vkDestroyPipelineLayout(device, checkerboardPipelineLayout, nullptr);
    DestroyPipelines();
}

void RTGL1::ImageComposition::PrepareForRaster(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const GlobalUniform* uniform)
{
    ProcessCheckerboard(cmd, frameIndex, uniform);
}

void RTGL1::ImageComposition::Finalize( VkCommandBuffer      cmd,
                                        uint32_t             frameIndex,
                                        const GlobalUniform* uniform,
                                        const Tonemapping*   tonemapping,
                                        const Volumetric*    volumetric )
{
    SetupLpmParams( cmd );
    ApplyTonemapping( cmd, frameIndex, uniform, tonemapping, volumetric );
}

void RTGL1::ImageComposition::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::ImageComposition::ApplyTonemapping( VkCommandBuffer      cmd,
                                                uint32_t             frameIndex,
                                                const GlobalUniform* uniform,
                                                const Tonemapping*   tonemapping,
                                                const Volumetric*    volumetric )
{
    CmdLabel label(cmd, "Prefinal framebuf compose");


    // sync access
    framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);


    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, composePipeline);


    // bind desc sets
    VkDescriptorSet sets[] = {
        framebuffers->GetDescSet( frameIndex ),
        uniform->GetDescSet( frameIndex ),
        tonemapping->GetDescSet(),
        descSet,
        volumetric->GetDescSet( frameIndex ),
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            composePipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);


    // start compute shader
    uint32_t wgCountX = Utils::GetWorkGroupCount(uniform->GetData()->renderWidth, COMPUTE_COMPOSE_GROUP_SIZE_X);
    uint32_t wgCountY = Utils::GetWorkGroupCount(uniform->GetData()->renderHeight, COMPUTE_COMPOSE_GROUP_SIZE_Y);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
}

void RTGL1::ImageComposition::ProcessCheckerboard( VkCommandBuffer      cmd,
                                                   uint32_t             frameIndex,
                                                   const GlobalUniform* uniform )
{
    CmdLabel label(cmd, "Final framebuf checkerboard");


    // sync access
    framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_PRE_FINAL);


    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, checkerboardPipeline);


    // bind desc sets
    VkDescriptorSet sets[] =
    {
        framebuffers->GetDescSet(frameIndex),
        uniform->GetDescSet(frameIndex)
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            checkerboardPipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);


    // start compute shader
    uint32_t wgCountX = Utils::GetWorkGroupCount(uniform->GetData()->renderWidth, COMPUTE_COMPOSE_GROUP_SIZE_X); 
    uint32_t wgCountY = Utils::GetWorkGroupCount(uniform->GetData()->renderHeight, COMPUTE_COMPOSE_GROUP_SIZE_Y);

    vkCmdDispatch(cmd, wgCountX, wgCountY, 1);
}

VkPipelineLayout RTGL1::ImageComposition::CreatePipelineLayout(
    VkDevice device,
    VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
    const char *pDebugName)
{
    VkPipelineLayoutCreateInfo info = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = setLayoutCount,
        .pSetLayouts = pSetLayouts,
    };

    VkPipelineLayout layout;
    VkResult r = vkCreatePipelineLayout(device, &info, nullptr, &layout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pDebugName);

    return layout;
}

void RTGL1::ImageComposition::CreateDescriptors()
{
    VkResult r;
    {
        VkDescriptorSetLayoutBinding binding =
        {
            .binding = BINDING_LPM_PARAMS,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding,
        };

        r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "LPM Desc set layout");
    }
    {
        VkDescriptorPoolSize poolSize =
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo poolInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };

        r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "LPM Desc pool");
    }
    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descLayout,
        };

        r = vkAllocateDescriptorSets(device, &allocInfo, &descSet);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "LPM Desc set");
    }
    {
        VkDescriptorBufferInfo bfInfo =
        {
            .buffer = lpmParams->GetDeviceLocal(),
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet wrt =
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descSet,
            .dstBinding = BINDING_LUM_HISTOGRAM,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bfInfo,
        };

        vkUpdateDescriptorSets(device, 1, &wrt, 0, nullptr);
    }
}

void RTGL1::ImageComposition::CreatePipelines(const ShaderManager *shaderManager)
{
    {
        VkComputePipelineCreateInfo plInfo = {};
        plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        plInfo.layout = composePipelineLayout;
        plInfo.stage = shaderManager->GetStageInfo("CPrepareFinal");

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &composePipeline);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, composePipeline, VK_OBJECT_TYPE_PIPELINE, "Composition pipeline");
    }

    {
        VkComputePipelineCreateInfo plInfo = {};
        plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        plInfo.layout = checkerboardPipelineLayout;
        plInfo.stage = shaderManager->GetStageInfo("CCheckerboard");

        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &checkerboardPipeline);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, checkerboardPipeline, VK_OBJECT_TYPE_PIPELINE, "Checkerboard pipeline");
    }
}

void RTGL1::ImageComposition::DestroyPipelines()
{
    vkDestroyPipeline(device, composePipeline, nullptr);
    composePipeline = VK_NULL_HANDLE;

    vkDestroyPipeline(device, checkerboardPipeline, nullptr);
    checkerboardPipeline = VK_NULL_HANDLE;
}

#define A_CPU 1
#include "Shaders/LPM/ffx_a.h"

namespace
{
    void LpmSetupOut(AU1 i, inAU4 v)
    {
        // this function version must not be called, only one with pContext
        assert(0);
    }

    void LpmSetupOut(void *pContext, AU1 i, const inAU4 value)
    {
        AU1* ctl = static_cast<AU1*>(pContext);

        assert(i < 24);

        ctl[i * 4 + 0] = value[0];
        ctl[i * 4 + 1] = value[1];
        ctl[i * 4 + 2] = value[2];
        ctl[i * 4 + 3] = value[3];
    }
}

#include "Shaders/LPM/ffx_lpm.h"
#include "Shaders/LPM/LpmSetupCustom.inl"

void RTGL1::ImageComposition::SetupLpmParams(VkCommandBuffer cmd)
{
    if (lpmParamsInited)
    {
        return;
    }

    void* pContext = lpmParams->GetMapped(0);
    #define LPM_RG_CONTEXT pContext,

    {
        varAF3( saturation ) = initAF3( -0.1f, -0.1f, -0.1f );
        varAF3( crosstalk )  = initAF3( 1.0f, 1.0f / 8.0f, 1.0f / 16.0f );
        LpmSetup( LPM_RG_CONTEXT false,
                  LPM_CONFIG_709_709,
                  LPM_COLORS_709_709,
                  0.0f,   // softGap
                  256.0f, // hdrMax
                  8.0f,   // exposure
                  0.1f,   // contrast
                  1.0f,   // shoulder contrast
                  saturation,
                  crosstalk );
    }

    lpmParams->CopyFromStaging(cmd, 0, LPM_BUFFER_SIZE);

    VkBufferMemoryBarrier2KHR b =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .buffer = lpmParams->GetDeviceLocal(),
        .offset = 0,
        .size = LPM_BUFFER_SIZE,
    };

    VkDependencyInfoKHR info =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &b,
    };

    svkCmdPipelineBarrier2KHR(cmd, &info);

    lpmParamsInited = true;
}
