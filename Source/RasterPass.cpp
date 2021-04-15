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

#include "RasterPass.h"

#include "Generated/ShaderCommonCFramebuf.h"
#include "Rasterizer.h"


constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;


RTGL1::RasterPass::RasterPass(
    VkDevice _device, 
    VkPipelineLayout _pipelineLayout,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<Framebuffers> &_storageFramebuffers)
:
    device(_device),
    rasterRenderPass(VK_NULL_HANDLE),
    rasterWidth(0),
    rasterHeight(0),
    rasterFramebuffers{},
    rasterSkyFramebuffers{},
    depthImages{},
    depthViews{},
    depthMemory{}
{
    CreateRasterRenderPass(ShFramebuffers_Formats[FB_IMAGE_INDEX_FINAL], DEPTH_FORMAT);

    rasterPipelines = std::make_shared<RasterizerPipelines>(device, _pipelineLayout, rasterRenderPass);
    rasterPipelines->SetShaders(_shaderManager.get(), "VertRasterizer", "FragRasterizerDepth");

    depthCopying = std::make_shared<DepthCopying>(device, DEPTH_FORMAT, _shaderManager, _storageFramebuffers);
}

RTGL1::RasterPass::~RasterPass()
{
    vkDestroyRenderPass(device, rasterRenderPass, nullptr);
    DestroyFramebuffers();
}

void RTGL1::RasterPass::PrepareForFinal(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<Framebuffers> &storageFramebuffers, 
    bool werePrimaryTraced)
{
    assert(rasterWidth > 0 && rasterHeight > 0);

    // Firstly, copy data from storage buffer to depth buffer,
    // and only after getting correct depth buffer, draw the geometry.
    // If no primary rays were traced, then just clear depth buffer without copying.
    depthCopying->Process(cmd, frameIndex, storageFramebuffers, rasterWidth, rasterHeight, !werePrimaryTraced);
}

void RTGL1::RasterPass::CreateFramebuffers(uint32_t renderWidth, uint32_t renderHeight,
                                           const std::shared_ptr<Framebuffers> &storageFramebuffers,
                                           const std::shared_ptr<MemoryAllocator> &allocator)
{
    CreateDepthBuffers(renderWidth, renderHeight, allocator);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(rasterFramebuffers[i] == VK_NULL_HANDLE);
        assert(rasterSkyFramebuffers[i] == VK_NULL_HANDLE);

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = rasterRenderPass;
        fbInfo.width = renderWidth;
        fbInfo.height = renderHeight;
        fbInfo.layers = 1;

        VkImageView attchs[] =
        {
            VK_NULL_HANDLE,
            depthViews[i]
        };

        fbInfo.attachmentCount = sizeof(attchs) / sizeof(attchs[0]);
        fbInfo.pAttachments = attchs;

        {
            attchs[0] = storageFramebuffers->GetImageView(FB_IMAGE_INDEX_FINAL, i);

            VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &rasterFramebuffers[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, rasterFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer raster framebuffer");
        }

        {
            attchs[0] = storageFramebuffers->GetImageView(FB_IMAGE_INDEX_ALBEDO, i);

            VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &rasterSkyFramebuffers[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, rasterSkyFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer raster sky framebuffer");
        }
    }

    depthCopying->CreateFramebuffers(depthViews, renderWidth, renderHeight);

    this->rasterWidth = renderWidth;
    this->rasterHeight = renderHeight;
}

void RTGL1::RasterPass::DestroyFramebuffers()
{
    depthCopying->DestroyFramebuffers();

    DestroyDepthBuffers();

    for (VkFramebuffer &fb : rasterFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }

    for (VkFramebuffer &fb : rasterSkyFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
}

VkRenderPass RTGL1::RasterPass::GetRasterRenderPass() const
{
    return rasterRenderPass;
}

const std::shared_ptr<RTGL1::RasterizerPipelines> &RTGL1::RasterPass::GetRasterPipelines() const
{
    return rasterPipelines;
}

uint32_t RTGL1::RasterPass::GetRasterWidth() const
{
    return rasterWidth;
}

uint32_t RTGL1::RasterPass::GetRasterHeight() const
{
    return rasterHeight;
}

VkFramebuffer RTGL1::RasterPass::GetFramebuffer(uint32_t frameIndex) const
{
    return rasterFramebuffers[frameIndex];
}

VkFramebuffer RTGL1::RasterPass::GetSkyFramebuffer(uint32_t frameIndex) const
{
    return rasterSkyFramebuffers[frameIndex];
}

void RTGL1::RasterPass::OnShaderReload(const ShaderManager *shaderManager)
{
    rasterPipelines->Clear();
    rasterPipelines->SetShaders(shaderManager, "VertRasterizer", "FragRasterizerDepth");

    depthCopying->OnShaderReload(shaderManager);
}

void RTGL1::RasterPass::CreateRasterRenderPass(VkFormat finalImageFormat, VkFormat depthImageFormat)
{
    const int attchCount = 2;
    VkAttachmentDescription attchs[attchCount] = {};

    auto &colorAttch = attchs[0];
    colorAttch.format = finalImageFormat;
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    auto &depthAttch = attchs[1];
    depthAttch.format = depthImageFormat;
    depthAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttch.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttch.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttch.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;


    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;


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
    passInfo.attachmentCount = attchCount;
    passInfo.pAttachments = attchs;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;

    VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &rasterRenderPass);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rasterRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Rasterizer raster render pass");
}

void RTGL1::RasterPass::CreateDepthBuffers(uint32_t width, uint32_t height, const std::shared_ptr<MemoryAllocator> &allocator)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(depthImages[i] == VK_NULL_HANDLE);
        assert(depthViews[i] == VK_NULL_HANDLE);
        assert(depthMemory[i] == VK_NULL_HANDLE);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.format = DEPTH_FORMAT;
        imageInfo.extent = { width, height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult r = vkCreateImage(device, &imageInfo, nullptr, &depthImages[i]);
        VK_CHECKERROR(r);
        SET_DEBUG_NAME(device, depthImages[i], VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Rasterizer raster pass depth image" );


        // allocate dedicated memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, depthImages[i], &memReqs);

        depthMemory[i] = allocator->AllocDedicated(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        SET_DEBUG_NAME(device, depthMemory[i], VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, "Rasterizer raster pass depth memory");

        if (depthMemory[i] == VK_NULL_HANDLE)
        {
            vkDestroyImage(device, depthImages[i], nullptr);
            depthImages[i] = VK_NULL_HANDLE;

            return;
        }

        r = vkBindImageMemory(device, depthImages[i], depthMemory[i], 0);
        VK_CHECKERROR(r);


        // create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = DEPTH_FORMAT;
        viewInfo.subresourceRange = {};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.image = depthImages[i];

        r = vkCreateImageView(device, &viewInfo, nullptr, &depthViews[i]);
        VK_CHECKERROR(r);
        SET_DEBUG_NAME(device, depthViews[i], VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Rasterizer raster pass depth image view");
    }
}

void RTGL1::RasterPass::DestroyDepthBuffers()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(depthImages[i] != VK_NULL_HANDLE);
        assert(depthViews[i] != VK_NULL_HANDLE);
        assert(depthMemory[i] != VK_NULL_HANDLE);

        vkDestroyImage(device, depthImages[i], nullptr);
        vkDestroyImageView(device, depthViews[i], nullptr);
        vkFreeMemory(device, depthMemory[i], nullptr);

        depthImages[i] = VK_NULL_HANDLE;
        depthViews[i] = VK_NULL_HANDLE;
        depthMemory[i] = VK_NULL_HANDLE;
    }
}
