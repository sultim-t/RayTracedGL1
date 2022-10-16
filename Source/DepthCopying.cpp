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

#include "DepthCopying.h"

namespace
{
constexpr const char* SHADER_VERT = "VertFullscreenQuad";
constexpr const char* SHADER_FRAG = "FragDepthCopying";
}


RTGL1::DepthCopying::DepthCopying( VkDevice             _device,
                                   VkFormat             _depthFormat,
                                   const ShaderManager& _shaderManager,
                                   const Framebuffers&  _storageFramebuffers )
    : device( _device )
    , renderPass( VK_NULL_HANDLE )
    , framebuffers{}
    , pipelineLayout( VK_NULL_HANDLE )
    , pipeline( VK_NULL_HANDLE )
{
    CreateRenderPass( _depthFormat );
    CreatePipelineLayout( _storageFramebuffers.GetDescSetLayout() );
    CreatePipeline( &_shaderManager );
}

RTGL1::DepthCopying::~DepthCopying()
{
    vkDestroyRenderPass( device, renderPass, nullptr );
    vkDestroyPipeline( device, pipeline, nullptr );
    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );

    DestroyFramebuffers();
}

void RTGL1::DepthCopying::Process( VkCommandBuffer     cmd,
                                   uint32_t            frameIndex,
                                   const Framebuffers& storageFramebuffers,
                                   uint32_t            width,
                                   uint32_t            height,
                                   bool                justClear )
{
    assert( renderPass && framebuffers[ frameIndex ] && pipeline && pipelineLayout );

    VkDescriptorSet descSets[] = {
        storageFramebuffers.GetDescSet( frameIndex ),
    };

    VkRect2D renderArea = {
        .offset = { 0, 0 },
        .extent = { width, height },
    };

    VkViewport viewport = {
        .x        = 0,
        .y        = 0,
        .width    = float( width ),
        .height   = float( height ),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRenderPassBeginInfo beginInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = renderPass,
        .framebuffer     = framebuffers[ frameIndex ],
        .renderArea      = renderArea,
        .clearValueCount = 0,
    };

    vkCmdBeginRenderPass( cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );

    if( !justClear )
    {
        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

        vkCmdSetScissor( cmd, 0, 1, &renderArea );
        vkCmdSetViewport( cmd, 0, 1, &viewport );

        vkCmdBindDescriptorSets( cmd,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout,
                                 0,
                                 std::size( descSets ),
                                 descSets,
                                 0,
                                 nullptr );

        uint32_t push[] = { width, height };
        vkCmdPushConstants(
            cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), push );

        vkCmdDraw( cmd, 6, 1, 0, 0 );
    }
    else
    {
        VkClearRect rect = {
            .rect           = {
                .offset = { 0, 0 },
                .extent = { width, height },
            },
            .baseArrayLayer = 0,
            .layerCount     = 1,
        };

        VkClearAttachment clear = {
            .aspectMask      = VK_IMAGE_ASPECT_DEPTH_BIT,
            .colorAttachment = 0,
            .clearValue      = { .depthStencil = { .depth = 1.0f } },
        };

        vkCmdClearAttachments( cmd, 1, &clear, 1, &rect );
    }

    vkCmdEndRenderPass( cmd );
}

void RTGL1::DepthCopying::OnShaderReload( const ShaderManager* shaderManager )
{
    vkDestroyPipeline( device, pipeline, nullptr );
    pipeline = VK_NULL_HANDLE;

    CreatePipeline( shaderManager );
}

void RTGL1::DepthCopying::CreateRenderPass( VkFormat depthFormat )
{
    VkAttachmentDescription depthAttch = {
        .format         = depthFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 0,
        .pDepthStencilAttachment = &depthRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo passInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &depthAttch,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkResult r = vkCreateRenderPass( device, &passInfo, nullptr, &renderPass );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, renderPass, VK_OBJECT_TYPE_RENDER_PASS, "Depth copying render pass" );
}

void RTGL1::DepthCopying::CreateFramebuffers( VkImageView pDepthAttchViews[ MAX_FRAMES_IN_FLIGHT ],
                                              uint32_t    width,
                                              uint32_t    height )
{
    assert( renderPass );

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( framebuffers[ i ] == VK_NULL_HANDLE );

        VkFramebufferCreateInfo fbInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = renderPass,
            .attachmentCount = 1,
            .pAttachments    = &pDepthAttchViews[ i ],
            .width           = width,
            .height          = height,
            .layers          = 1,
        };

        VkResult r = vkCreateFramebuffer( device, &fbInfo, nullptr, &framebuffers[ i ] );
        VK_CHECKERROR( r );
    }
}

void RTGL1::DepthCopying::DestroyFramebuffers()
{
    for( auto& f : framebuffers )
    {
        if( f != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, f, nullptr );
            f = VK_NULL_HANDLE;
        }
    }
}

void RTGL1::DepthCopying::CreatePipelineLayout( VkDescriptorSetLayout fbSetLayout )
{
    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof( uint32_t ) * 2,
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &fbSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push,
    };

    VkResult r = vkCreatePipelineLayout( device, &layoutInfo, nullptr, &pipelineLayout );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device,
                    pipelineLayout,
                    VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                    "Depth copying raster pipeline layout" );
}

void RTGL1::DepthCopying::CreatePipeline( const ShaderManager* shaderManager )
{
    assert( renderPass && pipelineLayout );
    assert( pipeline == VK_NULL_HANDLE );

    VkPipelineShaderStageCreateInfo stages[] = {
        shaderManager->GetStageInfo( SHADER_VERT ),
        shaderManager->GetStageInfo( SHADER_FRAG ),
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkVertexInputBindingDescription vertBinding = {
        .binding   = 0,
        .stride    = 0,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType                         = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions    = &vertBinding,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = nullptr,
        .scissorCount  = 1,
        .pScissors     = nullptr,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        // enable for depthWriteEnable
        .depthTestEnable = VK_TRUE,
        // write to depth buffer through gl_FragDepth
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttch = {
        .blendEnable    = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttch,
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size( dynamicStates ),
        .pDynamicStates    = dynamicStates,
    };

    VkGraphicsPipelineCreateInfo plInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = std::size( stages ),
        .pStages             = stages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &colorBlendState,
        .pDynamicState       = &dynamicInfo,
        .layout              = pipelineLayout,
        .renderPass          = renderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
    };

    VkResult r = vkCreateGraphicsPipelines( device, nullptr, 1, &plInfo, nullptr, &pipeline );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, pipeline, VK_OBJECT_TYPE_PIPELINE, "Rasterizer raster draw pipeline" );
}
