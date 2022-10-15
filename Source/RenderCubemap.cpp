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

#include "RenderCubemap.h"

#include <algorithm>

#include "Matrix.h"
#include "RasterizedDataCollector.h"
#include "Generated/ShaderCommonC.h"

namespace RTGL1
{
namespace
{
    constexpr VkFormat CUBEMAP_FORMAT       = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr VkFormat CUBEMAP_DEPTH_FORMAT = VK_FORMAT_D16_UNORM;

    struct RasterizedMultiviewPushConst
    {
        float    model[ 16 ];
        uint32_t packedColor;
        uint32_t textureIndex;

        explicit RasterizedMultiviewPushConst( const RasterizedDataCollector::DrawInfo& info )
            : model{}, packedColor( info.base_color ), textureIndex( info.base_textureA )
        {
            Matrix::ToMat4Transposed( model, info.transform );
        }
    };

    VkMemoryRequirements GetImageMemoryRequirements( VkDevice device, VkImage image )
    {
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements( device, image, &memReqs );
        return memReqs;
    }
}
}

RTGL1::RenderCubemap::RenderCubemap( VkDevice                    _device,
                                     MemoryAllocator&            _allocator,
                                     const ShaderManager&        _shaderManager,
                                     const TextureManager&       _textureManager,
                                     const GlobalUniform&        _uniform,
                                     const SamplerManager&       _samplerManager,
                                     CommandBufferManager&       _cmdManager,
                                     const RgInstanceCreateInfo& _instanceInfo )
    : device( _device )
    , pipelineLayout( VK_NULL_HANDLE )
    , multiviewRenderPass( VK_NULL_HANDLE )
    , cubemap{}
    , cubemapDepth{}
    , cubemapFramebuffer( VK_NULL_HANDLE )
    , cubemapSize( std::max( _instanceInfo.rasterizedSkyCubemapSize, 16u ) )
    , descSetLayout( VK_NULL_HANDLE )
    , descPool( VK_NULL_HANDLE )
    , descSet( VK_NULL_HANDLE )
{
    CreatePipelineLayout( _textureManager.GetDescSetLayout(), _uniform.GetDescSetLayout() );
    CreateRenderPass();
    InitPipelines( _shaderManager, cubemapSize, _instanceInfo.rasterizedVertexColorGamma );

    VkCommandBuffer cmd = _cmdManager.StartGraphicsCmd();
    {
        cubemap      = CreateAttch( _allocator, cmd, cubemapSize, false );
        cubemapDepth = CreateAttch( _allocator, cmd, cubemapSize, true );
    }
    _cmdManager.Submit( cmd );
    _cmdManager.WaitGraphicsIdle();

    CreateFramebuffer( cubemapSize );
    CreateDescriptors( _samplerManager );
}

RTGL1::RenderCubemap::~RenderCubemap()
{
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyDescriptorSetLayout( device, descSetLayout, nullptr );
    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
    vkDestroyRenderPass( device, multiviewRenderPass, nullptr );

    vkDestroyImage( device, cubemap.image, nullptr );
    vkDestroyImageView( device, cubemap.view, nullptr );
    vkFreeMemory( device, cubemap.memory, nullptr );

    vkDestroyImage( device, cubemapDepth.image, nullptr );
    vkDestroyImageView( device, cubemapDepth.view, nullptr );
    vkFreeMemory( device, cubemapDepth.memory, nullptr );

    vkDestroyFramebuffer( device, cubemapFramebuffer, nullptr );
}

void RTGL1::RenderCubemap::OnShaderReload( const ShaderManager* shaderManager )
{
    pipelines->OnShaderReload( shaderManager );
}

void RTGL1::RenderCubemap::Draw( VkCommandBuffer                                   cmd,
                                 uint32_t                                          frameIndex,
                                 const std::shared_ptr< RasterizedDataCollector >& skyDataCollector,
                                 const std::shared_ptr< TextureManager >&          textureManager,
                                 const std::shared_ptr< GlobalUniform >&           uniform )
{
    const auto& drawInfos = skyDataCollector->GetSkyDrawInfos();

    if( drawInfos.empty() )
    {
        return;
    }

    VkDescriptorSet descSets[] = {
        textureManager->GetDescSet( frameIndex ),
        uniform->GetDescSet( frameIndex ),
    };

    VkClearValue clearValues[] = {
        {
            .color = { .float32 = { 0.0f, 0.0f, 0.0f, 0.0f } },
        },
        {
            .depthStencil = { .depth = 1.0f },
        },
    };

    VkRenderPassBeginInfo beginInfo = {
        .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass        = multiviewRenderPass,
        .framebuffer       = cubemapFramebuffer,
        .renderArea      = {
            .offset = { 0, 0 },
            .extent = { cubemapSize, cubemapSize },
        },
        .clearValueCount   = std::size( clearValues ),
        .pClearValues      = clearValues,
    };

    vkCmdBeginRenderPass( cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );


    VkPipeline curPipeline =
        pipelines->BindPipelineIfNew( cmd, VK_NULL_HANDLE, drawInfos[ 0 ].pipelineState );

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelines->GetPipelineLayout(),
                             0,
                             std::size( descSets ),
                             descSets,
                             0,
                             nullptr );

    {
        VkDeviceSize offset       = 0;
        VkBuffer     vertexBuffer = skyDataCollector->GetVertexBuffer();
        VkBuffer     indexBuffer  = skyDataCollector->GetIndexBuffer();
        vkCmdBindVertexBuffers( cmd, 0, 1, &vertexBuffer, &offset );
        vkCmdBindIndexBuffer( cmd, indexBuffer, offset, VK_INDEX_TYPE_UINT32 );
    }

    for( const auto& info : drawInfos )
    {
        curPipeline = pipelines->BindPipelineIfNew( cmd, curPipeline, info.pipelineState );

        // push const
        {
            RasterizedMultiviewPushConst push( info );

            vkCmdPushConstants( cmd,
                                pipelines->GetPipelineLayout(),
                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                0,
                                sizeof( push ),
                                &push );
        }

        // draw
        if( info.indexCount > 0 )
        {
            vkCmdDrawIndexed( cmd, info.indexCount, 1, info.firstIndex, int32_t( info.firstVertex ), 0 );
        }
        else
        {
            vkCmdDraw( cmd, info.vertexCount, 1, info.firstVertex, 0 );
        }
    }

    vkCmdEndRenderPass( cmd );
}

VkDescriptorSetLayout RTGL1::RenderCubemap::GetDescSetLayout() const
{
    return descSetLayout;
}

VkDescriptorSet RTGL1::RenderCubemap::GetDescSet() const
{
    return descSet;
}

void RTGL1::RenderCubemap::CreatePipelineLayout( VkDescriptorSetLayout texturesSetLayout,
                                                 VkDescriptorSetLayout uniformSetLayout )
{
    VkDescriptorSetLayout setLayouts[] = {
        texturesSetLayout,
        uniformSetLayout,
    };

    VkPushConstantRange pushConst = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof( RasterizedMultiviewPushConst ),
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = std::size( setLayouts ),
        .pSetLayouts            = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConst,
    };

    VkResult r = vkCreatePipelineLayout( device, &layoutInfo, nullptr, &pipelineLayout );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME(
        device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Render cubemap pipeline layout" );
}

void RTGL1::RenderCubemap::CreateRenderPass()
{
    VkAttachmentDescription attchs[] = {
        {
            .format         = CUBEMAP_FORMAT,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        {
            .format         = CUBEMAP_DEPTH_FORMAT,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthRef = {
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorRef,
        .pDepthStencilAttachment = &depthRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    };

    // cubemap, 6 faces
    uint32_t                        viewMask   = 0b00111111;
    int32_t                         viewOffset = 0;

    VkRenderPassMultiviewCreateInfo multiview = {
        .sType                = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
        .subpassCount         = 1,
        .pViewMasks           = &viewMask,
        .dependencyCount      = 1,
        .pViewOffsets         = &viewOffset,
        // no correlation between cubemap faces
        .correlationMaskCount = 0,
        .pCorrelationMasks    = nullptr,
    };

    VkRenderPassCreateInfo passInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = &multiview,
        .attachmentCount = std::size( attchs ),
        .pAttachments    = attchs,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkResult r = vkCreateRenderPass( device, &passInfo, nullptr, &multiviewRenderPass );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device,
                    multiviewRenderPass,
                    VK_OBJECT_TYPE_RENDER_PASS,
                    "Render cubemap multiview render pass" );
}

void RTGL1::RenderCubemap::InitPipelines( const ShaderManager& shaderManager,
                                          uint32_t             sideSize,
                                          bool                 applyVertexColorGamma )
{
    VkViewport viewport = {
        .x        = 0,
        .y        = 0,
        .width    = static_cast< float >( sideSize ),
        .height   = static_cast< float >( sideSize ),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissors = {
        .offset = { 0, 0 },
        .extent = { sideSize, sideSize },
    };

    pipelines = std::make_shared< RasterizerPipelines >( device,
                                                         pipelineLayout,
                                                         multiviewRenderPass,
                                                         &shaderManager,
                                                         "VertDefaultMultiview",
                                                         "FragSky",
                                                         0,
                                                         applyVertexColorGamma,
                                                         &viewport,
                                                         &scissors );
}

RTGL1::RenderCubemap::Attachment RTGL1::RenderCubemap::CreateAttch( MemoryAllocator& allocator,
                                                                    VkCommandBuffer  cmd,
                                                                    uint32_t         sideSize,
                                                                    bool             isDepth )
{
    VkImage image;
    {
        VkImageCreateInfo imageInfo = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags       = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = isDepth ? CUBEMAP_DEPTH_FORMAT : CUBEMAP_FORMAT,
            .extent      = { sideSize, sideSize, 1 },
            .mipLevels   = 1,
            .arrayLayers = 6,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage = isDepth ? VkImageUsageFlags( VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT )
                             : VkImageUsageFlags( VK_IMAGE_USAGE_SAMPLED_BIT |
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ),
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkResult r = vkCreateImage( device, &imageInfo, nullptr, &image );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        image,
                        VK_OBJECT_TYPE_IMAGE,
                        isDepth ? "Render cubemap depth image" : "Render cubemap image" );
    }

    VkDeviceMemory memory;
    {
        memory = allocator.AllocDedicated( GetImageMemoryRequirements( device, image ),
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           MemoryAllocator::AllocType::DEFAULT,
                                           isDepth ? "Render cubemap depth memory"
                                                   : "Render cubemap image memory" );
        if( memory == VK_NULL_HANDLE )
        {
            vkDestroyImage( device, image, nullptr );
            return {};
        }

        VkResult r = vkBindImageMemory( device, image, memory, 0 );
        VK_CHECKERROR( r );
    }


    VkImageView view;
    {
        VkImageViewCreateInfo viewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = image,
            .viewType         = VK_IMAGE_VIEW_TYPE_CUBE,
            .format           = isDepth ? CUBEMAP_DEPTH_FORMAT : CUBEMAP_FORMAT,
            .subresourceRange = { .aspectMask =
                                      isDepth ? VkImageAspectFlags( VK_IMAGE_ASPECT_DEPTH_BIT )
                                              : VkImageAspectFlags( VK_IMAGE_ASPECT_COLOR_BIT ),
                                  .baseMipLevel   = 0,
                                  .levelCount     = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount     = 6 },
        };
        VkResult r = vkCreateImageView( device, &viewInfo, nullptr, &view );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        view,
                        VK_OBJECT_TYPE_IMAGE_VIEW,
                        isDepth ? "Render cubemap depth image view" : "Render cubemap image view" );
    }

    // make transition from undefined manually, so initialLayout can be specified
    VkImageMemoryBarrier barrier;

    if( isDepth )
    {
        barrier = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount     = 6 },
        };
    }
    else
    {
        barrier = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount     = 6 },
        };
    }

    vkCmdPipelineBarrier( cmd,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          0,
                          0,
                          nullptr,
                          0,
                          nullptr,
                          1,
                          &barrier );



    return Attachment{
        .image  = image,
        .view   = view,
        .memory = memory,
    };
}

void RTGL1::RenderCubemap::CreateFramebuffer( uint32_t sideSize )
{
    if( cubemap.image == VK_NULL_HANDLE || cubemap.view == VK_NULL_HANDLE ||
        cubemapDepth.image == VK_NULL_HANDLE || cubemapDepth.view == VK_NULL_HANDLE )
    {
        return;
    }

    VkImageView attchs[] = {
        cubemap.view,
        cubemapDepth.view,
    };

    VkFramebufferCreateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = multiviewRenderPass,
        .attachmentCount = 2,
        .pAttachments    = attchs,
        .width           = sideSize,
        .height          = sideSize,
        .layers          = 1,
    };

    VkResult r = vkCreateFramebuffer( device, &info, nullptr, &cubemapFramebuffer );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME(
        device, cubemapFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, "Render cubemap framebuffer" );
}

void RTGL1::RenderCubemap::CreateDescriptors( const SamplerManager& samplerManager )
{
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding         = BINDING_RENDER_CUBEMAP,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_ALL,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &descSetLayout );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        descSetLayout,
                        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "Render cubemap Desc set layout" );
    }
    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = 1,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME(
            device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Render cubemap Desc pool" );
    }
    {
        VkDescriptorSetAllocateInfo setInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descSetLayout,
        };

        VkResult r = vkAllocateDescriptorSets( device, &setInfo, &descSet );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Render cubemap desc set" );
    }
    {
        VkDescriptorImageInfo img = {
            .sampler     = samplerManager.GetSampler( RG_SAMPLER_FILTER_LINEAR,
                                                  RG_SAMPLER_ADDRESS_MODE_REPEAT,
                                                  RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            .imageView   = cubemap.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet wrt = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = descSet,
            .dstBinding      = BINDING_RENDER_CUBEMAP,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &img,
        };

        vkUpdateDescriptorSets( device, 1, &wrt, 0, nullptr );
    }
}
