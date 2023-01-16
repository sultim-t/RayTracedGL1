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

#include "PortalList.h"

#include "CmdLabel.h"
#include "RgException.h"

#include "Generated/ShaderCommonC.h"
static_assert( sizeof( RTGL1::ShPortalInstance ) % 16 == 0 );
// to avoid include
static_assert( RTGL1::detail::PORTAL_LIST_BITCOUNT == PORTAL_MAX_COUNT );


RTGL1::PortalList::PortalList( VkDevice _device, std::shared_ptr< MemoryAllocator > _allocator )
    : device( _device ), descPool{}, descSetLayout{}, descSet{}
{
    buffer = std::make_shared< AutoBuffer >( std::move( _allocator ) );
    buffer->Create( PORTAL_MAX_COUNT * sizeof( ShPortalInstance ),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    "Portals buffer" );

    CreateDescriptors();
}

RTGL1::PortalList::~PortalList()
{
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyDescriptorSetLayout( device, descSetLayout, nullptr );
}

/*void RTGL1::PortalList::Upload( uint32_t frameIndex, const RgPortalUploadInfo& info )
{
    if( info.portalIndex >= PORTAL_MAX_COUNT )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Portal index must be in [0, 62]" );
    }

    if( uploadedIndices.test( info.portalIndex ) )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "Portal with such index was already uploaded in this frame" );
    }

    ShPortalInstance src = {};
    {
        memcpy( src.inPosition, info.inPosition.data, 3 * sizeof( float ) );
        memcpy( src.outPosition, info.outPosition.data, 3 * sizeof( float ) );
        memcpy( src.outDirection, info.outDirection.data, 3 * sizeof( float ) );
        memcpy( src.outUp, info.outUp.data, 3 * sizeof( float ) );
    }

    auto* dstArr = buffer->GetMappedAs< ShPortalInstance* >( frameIndex );

    memcpy( &dstArr[ info.portalIndex ], &src, sizeof( ShPortalInstance ) );
}*/

void RTGL1::PortalList::SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    CmdLabel label( cmd, "Copying portal infos" );

    buffer->CopyFromStaging( cmd, frameIndex );
    uploadedIndices.reset();
}

VkDescriptorSet RTGL1::PortalList::GetDescSet( uint32_t frameIndex ) const
{
    return descSet;
}

VkDescriptorSetLayout RTGL1::PortalList::GetDescSetLayout() const
{
    return descSetLayout;
}

void RTGL1::PortalList::CreateDescriptors()
{
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding         = BINDING_PORTAL_INSTANCES,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = nullptr,
            .flags        = 0,
            .bindingCount = 1,
            .pBindings    = &binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &descSetLayout );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        descSetLayout,
                        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "Portals Desc set layout" );
    }
    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Portals Desc pool" );
    }
    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descSetLayout,
        };

        VkResult r = vkAllocateDescriptorSets( device, &allocInfo, &descSet );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Portals Desc set" );
    }

    VkDescriptorBufferInfo bufInfo = {
        .buffer = buffer->GetDeviceLocal(),
        .offset = 0,
        .range  = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet wrt = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descSet,
        .dstBinding      = BINDING_PORTAL_INSTANCES,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo     = &bufInfo,
    };

    vkUpdateDescriptorSets( device, 1, &wrt, 0, nullptr );
}
