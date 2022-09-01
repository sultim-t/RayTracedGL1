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

#include "Volumetric.h"

#include "CmdLabel.h"
#include "ShaderManager.h"
#include "Generated/ShaderCommonC.h"
#include "Utils.h"

namespace 
{
    // must be in sync with declaration in shaders
    constexpr VkFormat VOLUME_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
}

RTGL1::Volumetric::Volumetric( VkDevice              _device,
                               CommandBufferManager* _cmdManager,
                               MemoryAllocator*      _allocator,
                               const ShaderManager*  _shaderManager )
    : device( _device )
{
    CreateSampler();
    CreateImages( _cmdManager, _allocator );
    CreateDescriptors();
    UpdateDescriptors();
    CreatePipelineLayout();
    CreatePipelines( _shaderManager );
}

RTGL1::Volumetric::~Volumetric()
{
    vkDestroyDescriptorSetLayout( device, descLayout, nullptr );
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroySampler( device, volumeSampler, nullptr );
    for( auto i : volumeViews )
    {
        vkDestroyImageView( device, i, nullptr );
    }
    for( auto i : volumeMemory )
    {
        MemoryAllocator::FreeDedicated( device, i );
    }
    for( auto i : volumeImages )
    {
        vkDestroyImage( device, i, nullptr );
    }
    vkDestroyPipelineLayout( device, processPipelineLayout, nullptr );
    DestroyPipelines();
}

void RTGL1::Volumetric::Process( VkCommandBuffer cmd, uint32_t frameIndex )
{
    CmdLabel label( cmd, "Volumetric Process" );

    // sync
    {
        VkImageMemoryBarrier2 b = {
            .sType        = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext        = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            .srcAccessMask       = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = 0,
            .dstQueueFamilyIndex = 0,
            .image               = volumeImages[ frameIndex ],
            .subresourceRange    = {
                   .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                   .baseMipLevel   = 0,
                   .levelCount     = 1,
                   .baseArrayLayer = 0,
                   .layerCount     = 1,
            },
        };

        VkDependencyInfoKHR info = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &b,
        };

        svkCmdPipelineBarrier2KHR( cmd, &info );
    }

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processPipeline );

    VkDescriptorSet sets[] = {
        this->GetDescSet( frameIndex ),
    };

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_COMPUTE,
                             processPipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    uint32_t cs[] = {
        Utils::GetWorkGroupCountT( VOLUMETRIC_SIZE_X, COMPUTE_VOLUMETRIC_GROUP_SIZE_X ),
        Utils::GetWorkGroupCountT( VOLUMETRIC_SIZE_Y, COMPUTE_VOLUMETRIC_GROUP_SIZE_Y ),
        1,
    };
    vkCmdDispatch( cmd, cs[ 0 ], cs[ 1 ], cs[2] );
}

void RTGL1::Volumetric::BarrierToReadProcessed( VkCommandBuffer cmd, uint32_t frameIndex )
{
    VkImageMemoryBarrier2 b = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext         = nullptr,
        .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR | 
                         VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT_KHR,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image               = volumeImages[ frameIndex ],
        .subresourceRange    = {
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };

    VkDependencyInfoKHR info = {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &b,
    };

    svkCmdPipelineBarrier2KHR( cmd, &info );
}

void RTGL1::Volumetric::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::Volumetric::CreateSampler()
{
    VkSamplerCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 0,
        .compareEnable           = VK_FALSE,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult r = vkCreateSampler( device, &info, nullptr, &volumeSampler );
    VK_CHECKERROR( r );
}

void RTGL1::Volumetric::CreateImages( CommandBufferManager* cmdManager, MemoryAllocator* allocator )
{
    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    for( VkImage& dst : volumeImages )
    {
        VkImageCreateInfo info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_3D,
            .format        = VOLUME_FORMAT,
            .extent        = {
                .width  = VOLUMETRIC_SIZE_X,
                .height = VOLUMETRIC_SIZE_Y,
                .depth  = VOLUMETRIC_SIZE_Z,
            },
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkResult r = vkCreateImage( device, &info, nullptr, &dst );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, dst, VK_OBJECT_TYPE_IMAGE, "Volumetric Image" );
    }

    for( size_t i = 0; i < std::size( volumeMemory ); i++ )
    {
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements( device, volumeImages[ i ], &memReqs );

        volumeMemory[ i ] = allocator->AllocDedicated( memReqs,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                       MemoryAllocator::AllocType::DEFAULT,
                                                       "Volumetric Image memory" );

        VkResult r = vkBindImageMemory( device, volumeImages[ i ], volumeMemory[ i ], 0 );
        VK_CHECKERROR( r );
    }

    for( size_t i = 0; i < std::size( volumeViews ); i++ )
    {
        VkImageViewCreateInfo viewInfo = {
            .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image                           = volumeImages[ i ],
            .viewType                        = VK_IMAGE_VIEW_TYPE_3D,
            .format                          = VOLUME_FORMAT,
            .subresourceRange                = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };

        VkResult r = vkCreateImageView( device, &viewInfo, nullptr, &volumeViews[ i ] );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, volumeViews[ i ], VK_OBJECT_TYPE_IMAGE_VIEW, "Volumetric Image view" );

        // to general layout
        Utils::BarrierImage( cmd,
                             volumeImages[ i ],
                             0,
                             VK_ACCESS_SHADER_WRITE_BIT,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL );
    }

    cmdManager->Submit( cmd );
    cmdManager->WaitGraphicsIdle();
}

void RTGL1::Volumetric::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding         = BINDING_VOLUMETRIC_STORAGE,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = BINDING_VOLUMETRIC_SAMPLER,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = BINDING_VOLUMETRIC_SAMPLER_PREV,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = std::size( bindings ),
        .pBindings    = bindings,
    };

    r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &descLayout );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, descLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Volumetric Desc set layout" );

    VkDescriptorPoolSize poolSize = {
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = std::size( bindings ) * MAX_FRAMES_IN_FLIGHT,
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };

    r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Volumetric Desc pool" );

    for( auto& d : descSets )
    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descLayout,
        };

        r = vkAllocateDescriptorSets( device, &allocInfo, &d );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, d, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Volumetric Desc set" );
    }
}

void RTGL1::Volumetric::UpdateDescriptors()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        VkDescriptorImageInfo imgs[] = {
            {
                .sampler     = VK_NULL_HANDLE,
                .imageView   = volumeViews[ i ],
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler     = volumeSampler,
                .imageView   = volumeViews[ i ],
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler     = volumeSampler,
                .imageView   = volumeViews[ Utils::GetPreviousByModulo( i, MAX_FRAMES_IN_FLIGHT ) ],
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
        };
        
        VkWriteDescriptorSet wrts[] = {
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descSets[ i ],
                .dstBinding      = BINDING_VOLUMETRIC_STORAGE,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo      = &imgs[ 0 ],
            },
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descSets[ i ],
                .dstBinding      = BINDING_VOLUMETRIC_SAMPLER,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo      = &imgs[ 1 ],
            },
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descSets[ i ],
                .dstBinding      = BINDING_VOLUMETRIC_SAMPLER_PREV,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo      = &imgs[ 2 ],
            },
        };

        static_assert( std::size( wrts ) == std::size( imgs ) );

        vkUpdateDescriptorSets( device, std::size( wrts ), wrts, 0, nullptr );
    }
}

VkDescriptorSetLayout RTGL1::Volumetric::GetDescSetLayout() const
{
    return descLayout;
}

VkDescriptorSet RTGL1::Volumetric::GetDescSet( uint32_t frameIndex ) const
{
    return descSets[ frameIndex ];
}

void RTGL1::Volumetric::CreatePipelineLayout()
{
    VkDescriptorSetLayout sets[] = {
        this->GetDescSetLayout(),
    };

    VkPipelineLayoutCreateInfo info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = std::size( sets ),
        .pSetLayouts            = sets,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };

    VkResult r = vkCreatePipelineLayout( device, &info, nullptr, &processPipelineLayout );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME(
        device, processPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Volumetric Process pipeline layout" );
}

void RTGL1::Volumetric::CreatePipelines( const ShaderManager* shaderManager )
{
    assert( processPipeline == VK_NULL_HANDLE );

    VkComputePipelineCreateInfo info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext  = nullptr,
        .flags  = 0,
        .stage  = shaderManager->GetStageInfo( "CVolumetricProcess" ),
        .layout = processPipelineLayout,
    };

    VkResult r =
        vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, &processPipeline );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, processPipeline, VK_OBJECT_TYPE_PIPELINE, "Volumetric Process pipeline" );
}

void RTGL1::Volumetric::DestroyPipelines()
{
    vkDestroyPipeline( device, processPipeline, nullptr );
    processPipeline = VK_NULL_HANDLE;
}

