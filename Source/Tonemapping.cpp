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

#include "Tonemapping.h"

#include <vector>
#include <cmath>

#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"
#include "Utils.h"

RTGL1::Tonemapping::Tonemapping( VkDevice                                      _device,
                                 std::shared_ptr< Framebuffers >               _framebuffers,
                                 const std::shared_ptr< const ShaderManager >& _shaderManager,
                                 const std::shared_ptr< const GlobalUniform >& _uniform,
                                 const std::shared_ptr< MemoryAllocator >&     _allocator )
    : device( _device ), framebuffers( std::move( _framebuffers ) )
{
    CreateTonemappingBuffer( _allocator );
    CreateTonemappingDescriptors();

    std::vector< VkDescriptorSetLayout > setLayouts = { framebuffers->GetDescSetLayout(),
                                                        _uniform->GetDescSetLayout(),
                                                        tmDescSetLayout };

    CreatePipelineLayout( setLayouts.data(), setLayouts.size() );
    CreatePipelines( _shaderManager.get() );
}

RTGL1::Tonemapping::~Tonemapping()
{
    tmBuffer.Destroy();

    vkDestroyDescriptorPool( device, tmDescPool, nullptr );
    vkDestroyDescriptorSetLayout( device, tmDescSetLayout, nullptr );

    DestroyPipelines();
    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
}

void RTGL1::Tonemapping::CalculateExposure( VkCommandBuffer cmd,
                                            uint32_t        frameIndex,
                                            const std::shared_ptr< const GlobalUniform >& uniform )
{
    CmdLabel label( cmd, "Exposure" );

    // sync access to histogram buffer
    {
        VkBufferMemoryBarrier2 b = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            .buffer        = tmBuffer.GetBuffer(),
            .offset        = 0,
            .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dep = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &b,
        };

        svkCmdPipelineBarrier2KHR( cmd, &dep );
    }

    // sync access
    framebuffers->BarrierOne( cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_HISTOGRAM_INPUT );


    // bind desc sets
    VkDescriptorSet sets[]   = { framebuffers->GetDescSet( frameIndex ),
                               uniform->GetDescSet( frameIndex ),
                               tmDescSet };
    const uint32_t  setCount = sizeof( sets ) / sizeof( VkDescriptorSet );

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, setCount, sets, 0, nullptr );


    // histogram

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, histogramPipeline );

    // cover full render size
    uint32_t wgCountX = Utils::GetWorkGroupCount( uniform->GetData()->renderWidth,
                                                  COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_X );
    uint32_t wgCountY = Utils::GetWorkGroupCount( uniform->GetData()->renderHeight,
                                                  COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_Y );

    vkCmdDispatch( cmd, wgCountX, wgCountY, 1 );


    // sync access to histogram buffer
    {
        VkBufferMemoryBarrier2 b = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            .buffer        = tmBuffer.GetBuffer(),
            .offset        = 0,
            .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dep = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &b,
        };

        svkCmdPipelineBarrier2KHR( cmd, &dep );
    }


    // calculate average luminance
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, avgLuminancePipeline );
    // only one working group
    vkCmdDispatch( cmd, 1, 1, 1 );


    // sync access to histogram buffer to read in compute / raster
    {
        VkBufferMemoryBarrier2 b = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstStageMask =
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .buffer        = tmBuffer.GetBuffer(),
            .offset        = 0,
            .size          = VK_WHOLE_SIZE,
        };

        VkDependencyInfo dep = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &b,
        };

        svkCmdPipelineBarrier2KHR( cmd, &dep );
    }
}

VkDescriptorSetLayout RTGL1::Tonemapping::GetDescSetLayout() const
{
    return tmDescSetLayout;
}

VkDescriptorSet RTGL1::Tonemapping::GetDescSet() const
{
    return tmDescSet;
}

void RTGL1::Tonemapping::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::Tonemapping::CreateTonemappingBuffer(
    const std::shared_ptr< MemoryAllocator >& allocator )
{
    tmBuffer.Init( *allocator,
                   sizeof( ShTonemapping ),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   "Tonemapping buffer" );
}

void RTGL1::Tonemapping::CreateTonemappingDescriptors()
{
    VkResult                     r;

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding                      = BINDING_LUM_HISTOGRAM;
    binding.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount              = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &tmDescSetLayout );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device,
                    tmDescSetLayout,
                    VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                    "Tonemapping Desc set layout" );

    VkDescriptorPoolSize poolSize = {};
    poolSize.type                 = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount      = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets                    = 1;
    poolInfo.poolSizeCount              = 1;
    poolInfo.pPoolSizes                 = &poolSize;

    r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &tmDescPool );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, tmDescPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Tonemapping Desc pool" );

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool              = tmDescPool;
    allocInfo.descriptorSetCount          = 1;
    allocInfo.pSetLayouts                 = &tmDescSetLayout;

    r = vkAllocateDescriptorSets( device, &allocInfo, &tmDescSet );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, tmDescSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Tonemapping Desc set" );

    VkDescriptorBufferInfo bfInfo = {};
    bfInfo.buffer                 = tmBuffer.GetBuffer();
    bfInfo.offset                 = 0;
    bfInfo.range                  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet wrt = {};
    wrt.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrt.dstSet               = tmDescSet;
    wrt.dstBinding           = BINDING_LUM_HISTOGRAM;
    wrt.dstArrayElement      = 0;
    wrt.descriptorCount      = 1;
    wrt.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrt.pBufferInfo          = &bfInfo;

    vkUpdateDescriptorSets( device, 1, &wrt, 0, nullptr );
}

void RTGL1::Tonemapping::CreatePipelineLayout( VkDescriptorSetLayout* pSetLayouts,
                                               uint32_t               setLayoutCount )
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount             = setLayoutCount;
    plLayoutInfo.pSetLayouts                = pSetLayouts;

    VkResult r = vkCreatePipelineLayout( device, &plLayoutInfo, nullptr, &pipelineLayout );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME(
        device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Tonemapping pipeline layout" );
}

void RTGL1::Tonemapping::CreatePipelines( const ShaderManager* shaderManager )
{
    VkResult                    r;

    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout                      = pipelineLayout;

    {
        plInfo.stage = shaderManager->GetStageInfo( "CLuminanceHistogram" );

        r = vkCreateComputePipelines(
            device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &histogramPipeline );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        histogramPipeline,
                        VK_OBJECT_TYPE_PIPELINE,
                        "Tonemapping LuminanceHistogram pipeline" );
    }

    {
        plInfo.stage = shaderManager->GetStageInfo( "CLuminanceAvg" );

        r = vkCreateComputePipelines(
            device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &avgLuminancePipeline );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        avgLuminancePipeline,
                        VK_OBJECT_TYPE_PIPELINE,
                        "Tonemapping LuminanceAvg pipeline" );
    }
}

void RTGL1::Tonemapping::DestroyPipelines()
{
    vkDestroyPipeline( device, histogramPipeline, nullptr );
    vkDestroyPipeline( device, avgLuminancePipeline, nullptr );

    histogramPipeline    = VK_NULL_HANDLE;
    avgLuminancePipeline = VK_NULL_HANDLE;
}