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
constexpr VkFormat SCATTERING_VOLUME_FORMAT   = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat ILLUMINATION_VOLUME_FORMAT = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
}

RTGL1::Volumetric::Volumetric( VkDevice              _device,
                               CommandBufferManager& _cmdManager,
                               MemoryAllocator&      _allocator,
                               const ShaderManager&  _shaderManager,
                               const GlobalUniform&  _uniform,
                               const BlueNoise&      _rnd,
                               const Framebuffers&   _framebuffers )
    : device( _device )
{
    CreateSampler();
    CreateImages( _cmdManager, _allocator );
    CreateDescriptors();
    UpdateDescriptors();
    CreatePipelineLayouts( _uniform, _rnd, _framebuffers );
    CreatePipelines( _shaderManager );
}

RTGL1::Volumetric::~Volumetric()
{
    vkDestroyDescriptorSetLayout( device, descLayout, nullptr );
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroySampler( device, volumeSampler, nullptr );
    for( auto i : scattering )
    {
        vkDestroyImage( device, i.image, nullptr );
        vkDestroyImageView( device, i.view, nullptr );
        MemoryAllocator::FreeDedicated( device, i.memory );
    }
    vkDestroyImage( device, illumination.image, nullptr );
    vkDestroyImageView( device, illumination.view, nullptr );
    MemoryAllocator::FreeDedicated( device, illumination.memory );
    vkDestroyPipelineLayout( device, processPipelineLayout, nullptr );
    vkDestroyPipelineLayout( device, accumPipelineLayout, nullptr );
    DestroyPipelines();
}

void RTGL1::Volumetric::ProcessScattering( VkCommandBuffer      cmd,
                                           uint32_t             frameIndex,
                                           const GlobalUniform& uniform,
                                           const BlueNoise&     rnd,
                                           const Framebuffers&  framebuffers,
                                           float                maxHistoryLength )
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
            .image               = scattering[ frameIndex ].image,
            .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount     = 1 },
        };
        VkDependencyInfoKHR info = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &b,
        };
        svkCmdPipelineBarrier2KHR( cmd, &info );
    }

    // sum
    {
        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processPipeline );

        VkDescriptorSet sets[] = {
            this->GetDescSet( frameIndex ),
            uniform.GetDescSet( frameIndex ),
            rnd.GetDescSet(),
        };
        vkCmdBindDescriptorSets( cmd,
                                 VK_PIPELINE_BIND_POINT_COMPUTE,
                                 processPipelineLayout,
                                 0,
                                 std::size( sets ),
                                 sets,
                                 0,
                                 nullptr );

        vkCmdDispatch(
            cmd,
            Utils::GetWorkGroupCountT( VOLUMETRIC_SIZE_X, COMPUTE_VOLUMETRIC_GROUP_SIZE_X ),
            Utils::GetWorkGroupCountT( VOLUMETRIC_SIZE_Y, COMPUTE_VOLUMETRIC_GROUP_SIZE_Y ),
            1 );
    }

    // sync to read scattering 3D image for screen space accum / rasterized world geometry
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
            .image               = scattering[ frameIndex ].image,
            .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount     = 1 },
        };
        VkDependencyInfoKHR info = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &b,
        };
        svkCmdPipelineBarrier2KHR( cmd, &info );
    }

    // accumulate to screen space
    {
        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, accumPipeline );

        VkDescriptorSet sets[] = {
            this->GetDescSet( frameIndex ),
            uniform.GetDescSet( frameIndex ),
            rnd.GetDescSet(),
            framebuffers.GetDescSet( frameIndex ),
        };
        vkCmdBindDescriptorSets( cmd,
                                 VK_PIPELINE_BIND_POINT_COMPUTE,
                                 accumPipelineLayout,
                                 0,
                                 std::size( sets ),
                                 sets,
                                 0,
                                 nullptr );

        vkCmdPushConstants( cmd,
                            accumPipelineLayout,
                            VK_SHADER_STAGE_COMPUTE_BIT,
                            0,
                            sizeof( maxHistoryLength ),
                            &maxHistoryLength );

        vkCmdDispatch( cmd,
                       Utils::GetWorkGroupCount( uniform.GetData()->renderWidth,
                                                 COMPUTE_SCATTER_ACCUM_GROUP_SIZE_X ),
                       Utils::GetWorkGroupCount( uniform.GetData()->renderHeight,
                                                 COMPUTE_SCATTER_ACCUM_GROUP_SIZE_X ),
                       1 );
    }
}

void RTGL1::Volumetric::BarrierToReadIllumination( VkCommandBuffer cmd )
{
    VkImageMemoryBarrier2 b = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext         = nullptr,
        .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask =
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT_KHR,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image               = illumination.image,
        .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel   = 0,
                                 .levelCount     = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount     = 1 },
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
    CreatePipelines( *shaderManager );
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

void RTGL1::Volumetric::CreateImages( CommandBufferManager& cmdManager, MemoryAllocator& allocator )
{
    VkCommandBuffer cmd = cmdManager.StartGraphicsCmd();

    std::tuple< VolumeDef*, VkFormat, const char* > all[] = {
        { &scattering[ 0 ], SCATTERING_VOLUME_FORMAT, "Scattering Volume" },
        { &scattering[ 1 ], SCATTERING_VOLUME_FORMAT, "Scattering Volume" },
        { &illumination, ILLUMINATION_VOLUME_FORMAT, "Illumination Volume" },
    };

    for( auto& [ dst, format, debugName ] : all )
    {
        VkImageCreateInfo info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_3D,
            .format        = format,
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

        VkResult r = vkCreateImage( device, &info, nullptr, &dst->image );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, dst->image, VK_OBJECT_TYPE_IMAGE, debugName );


        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements( device, dst->image, &memReqs );

        dst->memory = allocator.AllocDedicated( memReqs,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                MemoryAllocator::AllocType::DEFAULT,
                                                debugName );

        r = vkBindImageMemory( device, dst->image, dst->memory, 0 );
        VK_CHECKERROR( r );


        VkImageViewCreateInfo viewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = dst->image,
            .viewType         = VK_IMAGE_VIEW_TYPE_3D,
            .format           = format,
            .subresourceRange = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel   = 0,
                                  .levelCount     = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount     = 1 },
        };

        r = vkCreateImageView( device, &viewInfo, nullptr, &dst->view );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, dst->view, VK_OBJECT_TYPE_IMAGE_VIEW, debugName );

        // to general layout
        Utils::BarrierImage( cmd,
                             dst->image,
                             0,
                             VK_ACCESS_SHADER_WRITE_BIT,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL );
    }

    cmdManager.Submit( cmd );
    cmdManager.WaitGraphicsIdle();
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
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding         = BINDING_VOLUMETRIC_SAMPLER_PREV,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding         = BINDING_VOLUMETRIC_ILLUMINATION,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = BINDING_VOLUMETRIC_ILLUMINATION_SAMPLER,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT,
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

    SET_DEBUG_NAME(
        device, descLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Volumetric Desc set layout" );

    std::vector< VkDescriptorPoolSize > poolSizes;
    for( auto& binding : bindings )
    {
        auto existingPoolSize = std::ranges::find_if( poolSizes, [ &binding ]( auto& poolSize ) {
            return poolSize.type == binding.descriptorType;
        } );

        if( existingPoolSize != poolSizes.end() )
        {
            existingPoolSize->descriptorCount += ( binding.descriptorCount * MAX_FRAMES_IN_FLIGHT );
            continue;
        }

        const VkDescriptorPoolSize poolSize = {
            .type            = binding.descriptorType,
            .descriptorCount = binding.descriptorCount * MAX_FRAMES_IN_FLIGHT,
        };
        poolSizes.push_back( poolSize );
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast< uint32_t >( std::size( poolSizes ) ),
        .pPoolSizes    = poolSizes.data(),
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
                .imageView   = scattering[ i ].view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler     = volumeSampler,
                .imageView   = scattering[ i ].view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler = volumeSampler,
                .imageView =
                    scattering[ Utils::GetPreviousByModulo( i, MAX_FRAMES_IN_FLIGHT ) ].view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler     = VK_NULL_HANDLE,
                .imageView   = illumination.view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .sampler     = volumeSampler,
                .imageView   = illumination.view,
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
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descSets[ i ],
                .dstBinding      = BINDING_VOLUMETRIC_ILLUMINATION,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo      = &imgs[ 3 ],
            },
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descSets[ i ],
                .dstBinding      = BINDING_VOLUMETRIC_ILLUMINATION_SAMPLER,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo      = &imgs[ 4 ],
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

void RTGL1::Volumetric::CreatePipelineLayouts( const GlobalUniform& uniform,
                                               const BlueNoise&     rnd,
                                               const Framebuffers&  framebuffers )
{
    {
        VkDescriptorSetLayout sets[] = {
            this->GetDescSetLayout(),
            uniform.GetDescSetLayout(),
            rnd.GetDescSetLayout(),
        };

        VkPipelineLayoutCreateInfo info = {
            .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext          = nullptr,
            .flags          = 0,
            .setLayoutCount = std::size( sets ),
            .pSetLayouts    = sets,
        };

        VkResult r = vkCreatePipelineLayout( device, &info, nullptr, &processPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        processPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Volumetric Process pipeline layout" );
    }
    {
        VkDescriptorSetLayout sets[] = {
            this->GetDescSetLayout(),
            uniform.GetDescSetLayout(),
            rnd.GetDescSetLayout(),
            framebuffers.GetDescSetLayout(),
        };

        VkPushConstantRange push = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = sizeof( float ),
        };

        VkPipelineLayoutCreateInfo info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = std::size( sets ),
            .pSetLayouts            = sets,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push,
        };

        VkResult r = vkCreatePipelineLayout( device, &info, nullptr, &accumPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        accumPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Volumetric Accum pipeline layout" );
    }
}

void RTGL1::Volumetric::CreatePipelines( const ShaderManager& shaderManager )
{
    assert( processPipeline == VK_NULL_HANDLE );
    assert( accumPipeline == VK_NULL_HANDLE );

    {
        VkComputePipelineCreateInfo info = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext  = nullptr,
            .flags  = 0,
            .stage  = shaderManager.GetStageInfo( "CVolumetricProcess" ),
            .layout = processPipelineLayout,
        };

        VkResult r =
            vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, &processPipeline );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, processPipeline, VK_OBJECT_TYPE_PIPELINE, "Volumetric Process pipeline" );
    }
    {
        VkComputePipelineCreateInfo info = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext  = nullptr,
            .flags  = 0,
            .stage  = shaderManager.GetStageInfo( "ScatterAccum" ),
            .layout = accumPipelineLayout,
        };

        VkResult r =
            vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, &accumPipeline );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, accumPipeline, VK_OBJECT_TYPE_PIPELINE, "Volumetric Accum pipeline" );
    }
}

void RTGL1::Volumetric::DestroyPipelines()
{
    vkDestroyPipeline( device, processPipeline, nullptr );
    processPipeline = VK_NULL_HANDLE;

    vkDestroyPipeline( device, accumPipeline, nullptr );
    accumPipeline = VK_NULL_HANDLE;
}
