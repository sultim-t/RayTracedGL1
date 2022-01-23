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
#include "RgException.h"
#include "Utils.h"


constexpr const char *VERT_SHADER = "VertRasterizer";
constexpr const char *FRAG_SHADER = "FragRasterizer";
constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr const char *DEPTH_FORMAT_NAME = "VK_FORMAT_X8_D24_UNORM_PACK32";


RTGL1::RasterPass::RasterPass(
    VkDevice _device, 
    VkPhysicalDevice _physDevice,
    VkPipelineLayout _pipelineLayout,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<Framebuffers> &_storageFramebuffers,
    const RgInstanceCreateInfo &_instanceInfo)
:
    device(_device),
    rasterRenderPass(VK_NULL_HANDLE),
    rasterSkyRenderPass(VK_NULL_HANDLE),
    rasterWidth(0),
    rasterHeight(0),
    rasterFramebuffers{},
    rasterSkyFramebuffers{},
    depthImages{},
    depthViews{},
    depthMemory{}
{
    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(_physDevice, DEPTH_FORMAT, &props);
    if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
    {
        using namespace std::string_literals;
        throw RgException(RG_GRAPHICS_API_ERROR, "Depth format is not supported: "s + DEPTH_FORMAT_NAME);
    }

    CreateRasterRenderPass(ShFramebuffers_Formats[FB_IMAGE_INDEX_FINAL], ShFramebuffers_Formats[FB_IMAGE_INDEX_ALBEDO], DEPTH_FORMAT);

    rasterPipelines = std::make_shared<RasterizerPipelines>(device, _pipelineLayout, rasterRenderPass, _instanceInfo.rasterizedVertexColorGamma);
    rasterPipelines->SetShaders(_shaderManager.get(), VERT_SHADER, FRAG_SHADER);

    rasterSkyPipelines= std::make_shared<RasterizerPipelines>(device, _pipelineLayout, rasterSkyRenderPass, _instanceInfo.rasterizedVertexColorGamma);
    rasterSkyPipelines->SetShaders(_shaderManager.get(), VERT_SHADER, FRAG_SHADER);

    depthCopying = std::make_shared<DepthCopying>(device, DEPTH_FORMAT, _shaderManager, _storageFramebuffers);
}

RTGL1::RasterPass::~RasterPass()
{
    vkDestroyRenderPass(device, rasterRenderPass, nullptr);
    vkDestroyRenderPass(device, rasterSkyRenderPass, nullptr);
    DestroyFramebuffers();
}

void RTGL1::RasterPass::PrepareForFinal(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<Framebuffers> &storageFramebuffers, 
    bool werePrimaryTraced)
{
    assert(rasterWidth > 0 && rasterHeight > 0);

    // firstly, copy data from storage buffer to depth buffer,
    // and only after getting correct depth buffer, draw the geometry
    // if no primary rays were traced, just clear depth buffer without copying
    depthCopying->Process(cmd, frameIndex, storageFramebuffers, rasterWidth, rasterHeight, !werePrimaryTraced);
}

void RTGL1::RasterPass::CreateFramebuffers(uint32_t renderWidth, uint32_t renderHeight,
                                           const std::shared_ptr<Framebuffers> &storageFramebuffers,
                                           const std::shared_ptr<MemoryAllocator> &allocator,
                                           const std::shared_ptr<CommandBufferManager> &cmdManager)
{
    CreateDepthBuffers(renderWidth, renderHeight, allocator, cmdManager);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(rasterFramebuffers[i] == VK_NULL_HANDLE);
        assert(rasterSkyFramebuffers[i] == VK_NULL_HANDLE);

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.width = renderWidth;
        fbInfo.height = renderHeight;
        fbInfo.layers = 1;

        VkImageView attchs[] =
        {
            VK_NULL_HANDLE,
            depthViews[i]
        };

        fbInfo.attachmentCount = std::size(attchs);
        fbInfo.pAttachments = attchs;

        {
            fbInfo.renderPass = rasterRenderPass;

            attchs[0] = storageFramebuffers->GetImageView(FB_IMAGE_INDEX_FINAL, i);

            VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &rasterFramebuffers[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, rasterFramebuffers[i], VK_OBJECT_TYPE_FRAMEBUFFER, "Rasterizer raster framebuffer");
        }

        {
            fbInfo.renderPass = rasterSkyRenderPass;

            attchs[0] = storageFramebuffers->GetImageView(FB_IMAGE_INDEX_ALBEDO, i);

            VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &rasterSkyFramebuffers[i]);
            VK_CHECKERROR(r);

            SET_DEBUG_NAME(device, rasterSkyFramebuffers[i], VK_OBJECT_TYPE_FRAMEBUFFER, "Rasterizer raster sky framebuffer");
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

VkRenderPass RTGL1::RasterPass::GetSkyRasterRenderPass() const
{
    return rasterSkyRenderPass;
}

const std::shared_ptr<RTGL1::RasterizerPipelines> &RTGL1::RasterPass::GetRasterPipelines() const
{
    return rasterPipelines;
}

const std::shared_ptr<RTGL1::RasterizerPipelines> &RTGL1::RasterPass::GetSkyRasterPipelines() const
{
    return rasterSkyPipelines;
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
    rasterSkyPipelines->Clear();

    rasterPipelines->SetShaders(shaderManager, VERT_SHADER, FRAG_SHADER);
    rasterSkyPipelines->SetShaders(shaderManager, VERT_SHADER, FRAG_SHADER);

    depthCopying->OnShaderReload(shaderManager);
}

void RTGL1::RasterPass::CreateRasterRenderPass(VkFormat finalImageFormat, VkFormat skyFinalImageFormat, VkFormat depthImageFormat)
{
    const int attchCount = 2;
    VkAttachmentDescription attchs[attchCount] = {};

    auto &colorAttch = attchs[0];
    // will be overwritten
    colorAttch.format = VK_FORMAT_MAX_ENUM;
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
    // will be overwritten
    depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    depthAttch.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // depth image was already transitioned
    // by depthCopying for rasterRenderPass
    // and manually for rasterSkyRenderPass
    depthAttch.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttch.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    VkRenderPassCreateInfo passInfo = {};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passInfo.attachmentCount = attchCount;
    passInfo.pAttachments = attchs;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;

    {
        colorAttch.format = finalImageFormat;

        // load depth data from depthCopying
        depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &rasterRenderPass);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "Rasterizer raster render pass");
    }

    {
        colorAttch.format = skyFinalImageFormat;

        depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

        VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &rasterSkyRenderPass);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterSkyRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "Rasterizer raster sky render pass");
    }
}

void RTGL1::RasterPass::CreateDepthBuffers(uint32_t width, uint32_t height,
                                           const std::shared_ptr<MemoryAllocator> &allocator, 
                                           const std::shared_ptr<CommandBufferManager> &cmdManager)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(depthImages[i] == VK_NULL_HANDLE);
        assert(depthViews[i] == VK_NULL_HANDLE);
        assert(depthMemory[i] == VK_NULL_HANDLE);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.flags = 0;
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
        SET_DEBUG_NAME(device, depthImages[i], VK_OBJECT_TYPE_IMAGE, "Rasterizer raster pass depth image" );


        // allocate dedicated memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, depthImages[i], &memReqs);

        depthMemory[i] = allocator->AllocDedicated(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryAllocator::AllocType::DEFAULT, "Rasterizer raster pass depth memory");

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
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
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
        SET_DEBUG_NAME(device, depthViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, "Rasterizer raster pass depth image view");


        // make transition from undefined manually,
        // so depthAttch.initialLayout can be specified as DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

        VkImageMemoryBarrier imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.image = depthImages[i];
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &imageBarrier);

        cmdManager->Submit(cmd);
        cmdManager->WaitGraphicsIdle();
    }
}

void RTGL1::RasterPass::DestroyDepthBuffers()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert((depthImages[i] && depthViews[i] && depthMemory[i]) 
               || (!depthImages[i] && !depthViews[i] && !depthMemory[i]));

        if (depthImages[i] != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, depthImages[i], nullptr);
            vkDestroyImageView(device, depthViews[i], nullptr);
            vkFreeMemory(device, depthMemory[i], nullptr);

            depthImages[i] = VK_NULL_HANDLE;
            depthViews[i] = VK_NULL_HANDLE;
            depthMemory[i] = VK_NULL_HANDLE;
        }
    }
}
