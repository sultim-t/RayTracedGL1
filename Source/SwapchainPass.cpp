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

RTGL1::SwapchainPass::SwapchainPass(
    VkDevice _device, 
    VkPipelineLayout _pipelineLayout,
    VkFormat _surfaceFormat,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const RgInstanceCreateInfo &_instanceInfo)
:
    device(_device),
    swapchainRenderPass(VK_NULL_HANDLE),
    swapchainWidth(0),
    swapchainHeight(0)
{
    CreateSwapchainRenderPass(_surfaceFormat);

    swapchainPipelines = std::make_shared<RasterizerPipelines>(device, _pipelineLayout, swapchainRenderPass, _instanceInfo.rasterizedVertexColorGamma);
    swapchainPipelines->SetShaders(_shaderManager.get(), "VertRasterizer", "FragRasterizer");
}

RTGL1::SwapchainPass::~SwapchainPass()
{
    vkDestroyRenderPass(device, swapchainRenderPass, nullptr);
    DestroyFramebuffers();
}

void RTGL1::SwapchainPass::CreateFramebuffers(uint32_t newSwapchainWidth, uint32_t newSwapchainHeight,
                                              const VkImageView *pSwapchainAttchs, uint32_t swapchainAttchCount)
{
    // prepare framebuffers for drawing right into swapchain images
    swapchainFramebuffers.resize(swapchainAttchCount, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < swapchainAttchCount; i++)
    {
        assert(swapchainFramebuffers[i] == VK_NULL_HANDLE);

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = swapchainRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &pSwapchainAttchs[i];
        fbInfo.width = newSwapchainWidth;
        fbInfo.height = newSwapchainHeight;
        fbInfo.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &swapchainFramebuffers[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, swapchainFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer swapchain framebuffer");
    }

    this->swapchainWidth = newSwapchainWidth;
    this->swapchainHeight = newSwapchainHeight;
}

void RTGL1::SwapchainPass::DestroyFramebuffers()
{
    for (VkFramebuffer &fb : swapchainFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
}

void RTGL1::SwapchainPass::OnShaderReload(const ShaderManager *shaderManager)
{
    swapchainPipelines->Clear();
    swapchainPipelines->SetShaders(shaderManager, "VertRasterizer", "FragRasterizer");
}

VkRenderPass RTGL1::SwapchainPass::GetSwapchainRenderPass() const
{
    return swapchainRenderPass;
}

const std::shared_ptr<RTGL1::RasterizerPipelines> &RTGL1::SwapchainPass::GetSwapchainPipelines() const
{
    return swapchainPipelines;
}

uint32_t RTGL1::SwapchainPass::GetSwapchainWidth() const
{
    return swapchainWidth;
}

uint32_t RTGL1::SwapchainPass::GetSwapchainHeight() const
{
    return swapchainHeight;
}

VkFramebuffer RTGL1::SwapchainPass::GetSwapchainFramebuffer(uint32_t swapchainImageIndex) const
{
    if (swapchainImageIndex >= swapchainFramebuffers.size())
    {
        VK_NULL_HANDLE;
    }

    return swapchainFramebuffers[swapchainImageIndex];
}

void RTGL1::SwapchainPass::CreateSwapchainRenderPass(VkFormat surfaceFormat)
{
    VkAttachmentDescription colorAttch = {};
    colorAttch.format = surfaceFormat;
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = nullptr;


    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    VkRenderPassCreateInfo passInfo = {};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &colorAttch;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;

    VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &swapchainRenderPass);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, swapchainRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Rasterizer swapchain render pass");
}
