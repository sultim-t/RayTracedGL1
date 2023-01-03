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

#include "SwapchainPass.h"

RTGL1::SwapchainPass::SwapchainPass( VkDevice                    _device,
                                     VkPipelineLayout            _pipelineLayout,
                                     const ShaderManager&        _shaderManager,
                                     const RgInstanceCreateInfo& _instanceInfo )
    : device( _device ), swapchainRenderPass( VK_NULL_HANDLE ), fbPing{}, fbPong{}
{
    assert( ShFramebuffers_Formats[ FB_IMAGE_INDEX_UPSCALED_PING ] ==
            ShFramebuffers_Formats[ FB_IMAGE_INDEX_UPSCALED_PONG ] );

    swapchainRenderPass =
        CreateSwapchainRenderPass( ShFramebuffers_Formats[ FB_IMAGE_INDEX_UPSCALED_PING ] );

    swapchainPipelines =
        std::make_shared< RasterizerPipelines >( device,
                                                 _pipelineLayout,
                                                 swapchainRenderPass,
                                                 _shaderManager,
                                                 "VertDefault",
                                                 "FragSwapchain",
                                                 false,
                                                 _instanceInfo.rasterizedVertexColorGamma );
}

RTGL1::SwapchainPass::~SwapchainPass()
{
    vkDestroyRenderPass( device, swapchainRenderPass, nullptr );
    DestroyFramebuffers();
}

void RTGL1::SwapchainPass::CreateFramebuffers(
    uint32_t                               newSwapchainWidth,
    uint32_t                               newSwapchainHeight,
    const std::shared_ptr< Framebuffers >& storageFramebuffers )
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( fbPing[ i ] == VK_NULL_HANDLE && fbPong[ i ] == VK_NULL_HANDLE );

        {
            VkImageView v = storageFramebuffers->GetImageView(
                FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PING, 0 );

            VkFramebufferCreateInfo fbInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = swapchainRenderPass,
                .attachmentCount = 1,
                .pAttachments    = &v,
                .width           = newSwapchainWidth,
                .height          = newSwapchainHeight,
                .layers          = 1,
            };

            VkResult r = vkCreateFramebuffer( device, &fbInfo, nullptr, &fbPing[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            fbPing[ i ],
                            VK_OBJECT_TYPE_FRAMEBUFFER,
                            "Rasterizer swapchain ping framebuffer" );
        }
        {
            VkImageView v = storageFramebuffers->GetImageView(
                FramebufferImageIndex::FB_IMAGE_INDEX_UPSCALED_PONG, 0 );

            VkFramebufferCreateInfo fbInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = swapchainRenderPass,
                .attachmentCount = 1,
                .pAttachments    = &v,
                .width           = newSwapchainWidth,
                .height          = newSwapchainHeight,
                .layers          = 1,
            };

            VkResult r = vkCreateFramebuffer( device, &fbInfo, nullptr, &fbPong[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            fbPong[ i ],
                            VK_OBJECT_TYPE_FRAMEBUFFER,
                            "Rasterizer swapchain pong framebuffer" );
        }
    }
}

void RTGL1::SwapchainPass::DestroyFramebuffers()
{
    for( VkFramebuffer& fb : fbPing )
    {
        if( fb != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, fb, nullptr );
            fb = VK_NULL_HANDLE;
        }
    }
    for( VkFramebuffer& fb : fbPong )
    {
        if( fb != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, fb, nullptr );
            fb = VK_NULL_HANDLE;
        }
    }
}

void RTGL1::SwapchainPass::OnShaderReload( const ShaderManager* shaderManager )
{
    swapchainPipelines->OnShaderReload( shaderManager );
}

VkRenderPass RTGL1::SwapchainPass::GetSwapchainRenderPass() const
{
    return swapchainRenderPass;
}

const std::shared_ptr< RTGL1::RasterizerPipelines >& RTGL1::SwapchainPass::GetSwapchainPipelines()
    const
{
    return swapchainPipelines;
}

VkFramebuffer RTGL1::SwapchainPass::GetSwapchainFramebuffer( FramebufferImageIndex framebufIndex,
                                                             uint32_t frameIndex ) const
{
    assert( frameIndex < MAX_FRAMES_IN_FLIGHT );

    switch( framebufIndex )
    {
        case FB_IMAGE_INDEX_UPSCALED_PING: return fbPing[ frameIndex ];
        case FB_IMAGE_INDEX_UPSCALED_PONG: return fbPong[ frameIndex ];
        default: assert( 0 ); return VK_NULL_HANDLE;
    }
}

VkRenderPass RTGL1::SwapchainPass::CreateSwapchainRenderPass( VkFormat surfaceFormat ) const
{
    VkAttachmentDescription colorAttch = {
        .format         = surfaceFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorRef,
        .pDepthStencilAttachment = nullptr,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo passInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttch,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkRenderPass renderPass;
    VkResult     r = vkCreateRenderPass( device, &passInfo, nullptr, &renderPass );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME(
        device, renderPass, VK_OBJECT_TYPE_RENDER_PASS, "Rasterizer swapchain render pass" );
    return renderPass;
}
