// Copyright (c) 2020-2021 Sultim Tsyrendashiev
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

#include "Rasterizer.h"

#include <array>

#include "Swapchain.h"
#include "Matrix.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"
#include "Generated/ShaderCommonCFramebuf.h"


namespace RTGL1
{



struct RasterizedPushConst
{
    float vp[16];
    float c[4];
    uint32_t t;

    explicit RasterizedPushConst(const RasterizedDataCollector::DrawInfo &info, const float *defaultViewProj)
    {
        float model[16];
        Matrix::ToMat4Transposed(model, info.transform);

        static_assert(sizeof(*this) == 16 * sizeof(float) + 4 * sizeof(float) + sizeof(uint32_t), "");

        if (!info.isDefaultViewProjMatrix)
        {
            Matrix::Multiply(vp, model, info.viewProj);
        }
        else
        {
            Matrix::Multiply(vp, model, defaultViewProj);
        }

        memcpy(c, info.color, 4 * sizeof(float));
        t = info.textureIndex;
    }
};



Rasterizer::Rasterizer(
    VkDevice _device,
    const std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<TextureManager> &_textureManager,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<SamplerManager> &_samplerManager,
    std::shared_ptr<Framebuffers> _storageFramebuffers,
    VkFormat _surfaceFormat,
    const RgInstanceCreateInfo &_instanceInfo)
:
    device(_device),
    storageFramebuffers(std::move(_storageFramebuffers)),
    rasterRenderPass(VK_NULL_HANDLE),
    swapchainRenderPass(VK_NULL_HANDLE),
    rasterPipelineLayout(VK_NULL_HANDLE),
    swapchainPipelineLayout(VK_NULL_HANDLE),
    rasterFramebuffers{},
    rasterSkyFramebuffers{}
{
    collectorGeneral = std::make_shared<RasterizedDataCollectorGeneral>(device, _allocator, _textureManager, _instanceInfo.rasterizedMaxVertexCount, _instanceInfo.rasterizedMaxIndexCount);
    collectorSky = std::make_shared<RasterizedDataCollectorSky>(device, _allocator, _textureManager, _instanceInfo.rasterizedSkyMaxVertexCount, _instanceInfo.rasterizedSkyMaxIndexCount);

    CreateRasterRenderPass(ShFramebuffers_Formats[FB_IMAGE_INDEX_FINAL], ShFramebuffers_Formats[FB_IMAGE_INDEX_DEPTH]);
    CreateSwapchainRenderPass(_surfaceFormat);

    CreatePipelineLayouts(_textureManager->GetDescSetLayout(), storageFramebuffers->GetDescSetLayout());

    rasterPipelines = std::make_shared<RasterizerPipelines>(device, rasterPipelineLayout, rasterRenderPass);
    swapchainPipelines = std::make_shared<RasterizerPipelines>(device, swapchainPipelineLayout, swapchainRenderPass);

    rasterPipelines->SetShaders(_shaderManager.get(), "VertRasterizer", "FragRasterizerDepth");
    swapchainPipelines->SetShaders(_shaderManager.get(), "VertRasterizer", "FragRasterizer");

    renderCubemap = std::make_shared<RenderCubemap>(device, _allocator, _shaderManager, _textureManager, _uniform, _samplerManager, _instanceInfo.rasterizedSkyCubemapSize);
}

Rasterizer::~Rasterizer()
{
    vkDestroyRenderPass(device, rasterRenderPass, nullptr);
    vkDestroyRenderPass(device, swapchainRenderPass, nullptr);
    vkDestroyPipelineLayout(device, rasterPipelineLayout, nullptr);
    vkDestroyPipelineLayout(device, swapchainPipelineLayout, nullptr);
    DestroyRenderFramebuffers();
    DestroySwapchainFramebuffers();
}

void Rasterizer::PrepareForFrame(uint32_t frameIndex, bool requestRasterizedSkyGeometryReuse)
{
    collectorGeneral->Clear(frameIndex);

    if (!requestRasterizedSkyGeometryReuse)
    {
        collectorSky->Clear(frameIndex);
        isCubemapOutdated = true;
    }
}

void Rasterizer::Upload(uint32_t frameIndex, 
                        const RgRasterizedGeometryUploadInfo &uploadInfo, 
                        const float *viewProjection, const RgViewport *viewport)
{
    collectorGeneral->TryAddGeometry(frameIndex, uploadInfo, viewProjection, viewport);

    bool addedSkyGeom = collectorSky->TryAddGeometry(frameIndex, uploadInfo, viewProjection, viewport);

    // if trying to add geometry, but requestRasterizedSkyGeometryReuse was true
    if (addedSkyGeom && !isCubemapOutdated)
    {
        assert(0);
        isCubemapOutdated = true;
    }
}

void Rasterizer::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    collectorGeneral->CopyFromStaging(cmd, frameIndex);
    collectorSky->CopyFromStaging(cmd, frameIndex);
}

void Rasterizer::DrawSkyToCubemap(VkCommandBuffer cmd, uint32_t frameIndex, 
                                  const std::shared_ptr<TextureManager> &textureManager, 
                                  const std::shared_ptr<GlobalUniform> &uniform)
{
    if (isCubemapOutdated)
    {
        renderCubemap->Draw(cmd, frameIndex, collectorSky, textureManager, uniform);
        isCubemapOutdated = false;        
    }
}

void Rasterizer::DrawSkyToAlbedo(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, const RgFloat3D &skyViewerPos, float *proj)
{
    storageFramebuffers->Barrier(cmd, frameIndex, FB_IMAGE_INDEX_ALBEDO);

    float skyView[16];
    Matrix::SetNewViewerPosition(skyView, view, skyViewerPos.data);

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, skyView, proj);

    DrawParams params = 
    {
        cmd, frameIndex,
        rasterPipelines,
        collectorSky->GetSkyDrawInfos(),
        rasterRenderPass,
        rasterSkyFramebuffers[frameIndex],
        rasterFramebufferState,
        collectorSky->GetVertexBuffer(),
        collectorSky->GetIndexBuffer(),
        2,
        {
            textureManager->GetDescSet(frameIndex),
            storageFramebuffers->GetDescSet(frameIndex)
        },
        defaultViewProj
    };

    Draw(params);
}

void Rasterizer::DrawToFinalImage(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj)
{
    storageFramebuffers->Barrier(cmd, frameIndex, FB_IMAGE_INDEX_DEPTH);
    storageFramebuffers->Barrier(cmd, frameIndex, FB_IMAGE_INDEX_FINAL);
    // TODO: clear depth attachment if no rays were traced?

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    DrawParams params = 
    {
        cmd, frameIndex,
        rasterPipelines,
        collectorGeneral->GetRasterDrawInfos(),
        rasterRenderPass,
        rasterFramebuffers[frameIndex],
        rasterFramebufferState,
        collectorGeneral->GetVertexBuffer(),
        collectorGeneral->GetIndexBuffer(),
        2,
        {
            textureManager->GetDescSet(frameIndex),
            storageFramebuffers->GetDescSet(frameIndex)
        },
        defaultViewProj
    };

    Draw(params);
}

void Rasterizer::DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj)
{
    if (swapchainIndex >= swapchainFramebuffers.size())
    {
        return;
    }

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    DrawParams params = 
    {
        cmd, frameIndex,
        swapchainPipelines,
        collectorGeneral->GetSwapchainDrawInfos(),
        swapchainRenderPass,
        swapchainFramebuffers[swapchainIndex],
        swapchainFramebufferState,
        collectorGeneral->GetVertexBuffer(),
        collectorGeneral->GetIndexBuffer(),
        1,
        { 
            textureManager->GetDescSet(frameIndex),
            VK_NULL_HANDLE
        },
        defaultViewProj
    };

    Draw(params);
}

void Rasterizer::Draw(const DrawParams &drawParams)
{
    assert(drawParams.framebuffer != VK_NULL_HANDLE);

    if (drawParams.drawInfos.empty())
    {
        return;
    }


    VkCommandBuffer cmd = drawParams.cmd;

    const VkViewport &defaultViewport = drawParams.rasterAreaState.viewport;
    const VkRect2D &defaultRenderArea = drawParams.rasterAreaState.renderArea;

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = drawParams.renderPass;
    beginInfo.framebuffer = drawParams.framebuffer;
    beginInfo.renderArea = defaultRenderArea;
    beginInfo.clearValueCount = 0;
    beginInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);


    VkPipeline curPipeline = VK_NULL_HANDLE;
    BindPipelineIfNew(cmd, drawParams.drawInfos[0], drawParams.pipelines, curPipeline);


    VkDeviceSize offset = 0;

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawParams.pipelines->GetPipelineLayout(), 0,
        drawParams.descSetCount, drawParams.descSets,
        0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &drawParams.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, drawParams.indexBuffer, offset, VK_INDEX_TYPE_UINT32);


    vkCmdSetScissor(cmd, 0, 1, &defaultRenderArea);
    vkCmdSetViewport(cmd, 0, 1, &defaultViewport);

    VkViewport curViewport = defaultViewport;

    for (const auto &info : drawParams.drawInfos)
    {
        SetViewportIfNew(cmd, info, defaultViewport, curViewport);
        BindPipelineIfNew(cmd, info, drawParams.pipelines, curPipeline);

        // push const
        {
            RasterizedPushConst push(info, drawParams.defaultViewProj);

            vkCmdPushConstants(
                cmd, drawParams.pipelines->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(push),
                &push);
        }

        // draw
        if (info.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, info.indexCount, 1, info.firstIndex, info.firstVertex, 0);
        }
        else
        {
            vkCmdDraw(cmd, info.vertexCount, 1, info.firstVertex, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
}

void Rasterizer::SetViewportIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, 
                                  const VkViewport &defaultViewport, VkViewport &curViewport)
{
    const VkViewport &newViewport = info.isDefaultViewport ? defaultViewport : info.viewport;

    if (!Utils::AreViewportsSame(curViewport, newViewport))
    {
        vkCmdSetViewport(cmd, 0, 1, &newViewport);
        curViewport = newViewport;
    }
}

void Rasterizer::BindPipelineIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info,
    const std::shared_ptr<RasterizerPipelines> &pipelines, VkPipeline &curPipeline)
{
    // TODO: depth test / depth write, if there is a separate depth buffer,
    // blitting depth buffers is not allowed, so need to create full-quad pass that will fill target depth buffer,
    // which then will be used for depth test/write
    pipelines->BindPipelineIfNew(cmd, curPipeline, info.blendEnable, info.blendFuncSrc, info.blendFuncDst, false, false);
}

const std::shared_ptr<RenderCubemap> &Rasterizer::GetRenderCubemap() const
{
    return renderCubemap;
}

void Rasterizer::OnSwapchainCreate(const Swapchain *pSwapchain)
{
    CreateSwapchainFramebuffers(
        pSwapchain->GetWidth(), pSwapchain->GetHeight(),
        pSwapchain->GetImageViews(), pSwapchain->GetImageCount());
}

void Rasterizer::OnSwapchainDestroy()
{
    DestroySwapchainFramebuffers();
}

void Rasterizer::OnShaderReload(const ShaderManager *shaderManager)
{
    rasterPipelines->Clear();
    swapchainPipelines->Clear();

    // set reloaded shaders
    rasterPipelines->SetShaders(shaderManager, "VertRasterizer", "FragRasterizerDepth");
    swapchainPipelines->SetShaders(shaderManager, "VertRasterizer", "FragRasterizer");

    renderCubemap->OnShaderReload(shaderManager);
}

void Rasterizer::OnFramebuffersSizeChange(uint32_t width, uint32_t height)
{
    DestroyRenderFramebuffers();
    CreateRenderFramebuffers(width, height);
}

void Rasterizer::CreatePipelineLayouts(VkDescriptorSetLayout texturesSetLayout, VkDescriptorSetLayout fbSetLayout)
{
    VkDescriptorSetLayout setLayouts[] =
    {
        texturesSetLayout,
        fbSetLayout
    };

    VkPushConstantRange pushConst = {};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConst.offset = 0;
    pushConst.size = 16 * sizeof(float) + 4 * sizeof(float) + sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConst;

    {
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = setLayouts;

        VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &rasterPipelineLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterPipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Rasterizer raster pipeline layout");
    }

    {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = setLayouts;

        VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &swapchainPipelineLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, swapchainPipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Rasterizer swapchain pipeline layout");
    }
}

void Rasterizer::CreateRenderFramebuffers(uint32_t renderWidth, uint32_t renderHeight)
{
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

        const int attchCount = 1;
        VkImageView attchs[attchCount] = {};
        fbInfo.attachmentCount = attchCount;
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

    auto &rnvp = rasterFramebufferState.viewport;
    auto &rnra = rasterFramebufferState.renderArea;

    rnvp.x = 0.0f;
    rnvp.y = 0.0f;
    rnvp.minDepth = 0.0f;
    rnvp.maxDepth = 1.0f;
    rnvp.width = (float)renderWidth;
    rnvp.height = (float)renderHeight;

    rnra.offset = { 0, 0 };
    rnra.extent = { renderWidth, renderHeight };
}

void Rasterizer::CreateSwapchainFramebuffers(uint32_t swapchainWidth, uint32_t swapchainHeight,
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
        fbInfo.width = swapchainWidth;
        fbInfo.height = swapchainHeight;
        fbInfo.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &swapchainFramebuffers[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, swapchainFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer swapchain framebuffer");
    }

    auto &swvp = swapchainFramebufferState.viewport;
    auto &swra = swapchainFramebufferState.renderArea;

    swvp.x = 0.0f;
    swvp.y = 0.0f;
    swvp.minDepth = 0.0f;
    swvp.maxDepth = 1.0f;
    swvp.width = (float)swapchainWidth;
    swvp.height = (float)swapchainHeight;

    swra.offset = { 0, 0 };
    swra.extent = { swapchainWidth, swapchainHeight };
}

void Rasterizer::DestroyRenderFramebuffers()
{
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

void Rasterizer::DestroySwapchainFramebuffers()
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

void Rasterizer::CreateRasterRenderPass(VkFormat finalImageFormat, VkFormat depthImageFormat)
{
    const int attchCount = 1;
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

    /*auto &depthAttch = attchs[1];
    depthAttch.format = depthImageFormat; 
    depthAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttch.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    depthAttch.finalLayout = VK_IMAGE_LAYOUT_GENERAL;*/


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
    //subpass.pDepthStencilAttachment = &depthRef;


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

void Rasterizer::CreateSwapchainRenderPass(VkFormat surfaceFormat)
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

}
