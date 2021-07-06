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
#include "CmdLabel.h"


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
    VkPhysicalDevice _physDevice,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<TextureManager> &_textureManager,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<SamplerManager> &_samplerManager,
    std::shared_ptr<MemoryAllocator> _allocator,
    std::shared_ptr<Framebuffers> _storageFramebuffers,
    std::shared_ptr<CommandBufferManager> _cmdManager,
    VkFormat _surfaceFormat,
    const RgInstanceCreateInfo &_instanceInfo)
:
    device(_device),
    commonPipelineLayout(VK_NULL_HANDLE),
    allocator(std::move(_allocator)),
    cmdManager(std::move(_cmdManager)),
    storageFramebuffers(std::move(_storageFramebuffers)),
    isCubemapOutdated(true)
{
    collectorGeneral = std::make_shared<RasterizedDataCollectorGeneral>(device, allocator, _textureManager, _instanceInfo.rasterizedMaxVertexCount, _instanceInfo.rasterizedMaxIndexCount);
    collectorSky = std::make_shared<RasterizedDataCollectorSky>(device, allocator, _textureManager, _instanceInfo.rasterizedSkyMaxVertexCount, _instanceInfo.rasterizedSkyMaxIndexCount);

    CreatePipelineLayout(_textureManager->GetDescSetLayout());

    rasterPass = std::make_shared<RasterPass>(device, _physDevice, commonPipelineLayout, _shaderManager, storageFramebuffers, _instanceInfo);
    swapchainPass = std::make_shared<SwapchainPass>(device, commonPipelineLayout, _surfaceFormat, _shaderManager, _instanceInfo);
    renderCubemap = std::make_shared<RenderCubemap>(device, allocator, _shaderManager, _textureManager, _uniform, _samplerManager, cmdManager, _instanceInfo);
}

Rasterizer::~Rasterizer()
{
    vkDestroyPipelineLayout(device, commonPipelineLayout, nullptr);
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
    CmdLabel label(cmd, "Copying rasterizer data");

    collectorGeneral->CopyFromStaging(cmd, frameIndex);
    collectorSky->CopyFromStaging(cmd, frameIndex);
}

void Rasterizer::DrawSkyToCubemap(VkCommandBuffer cmd, uint32_t frameIndex, 
                                  const std::shared_ptr<TextureManager> &textureManager, 
                                  const std::shared_ptr<GlobalUniform> &uniform)
{
    if (isCubemapOutdated)
    {
        CmdLabel label(cmd, "Rasterized sky to cubemap");

        renderCubemap->Draw(cmd, frameIndex, collectorSky, textureManager, uniform);
        isCubemapOutdated = false;        
    }
}

void Rasterizer::DrawSkyToAlbedo(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, const float skyViewerPos[3], float *proj)
{
    CmdLabel label(cmd, "Rasterized sky to albedo framebuf");

    typedef FramebufferImageIndex FI;
    storageFramebuffers->BarrierOne(cmd, frameIndex, FI::FB_IMAGE_INDEX_ALBEDO);

    float skyView[16];
    Matrix::SetNewViewerPosition(skyView, view, skyViewerPos);

    float defaultSkyViewProj[16];
    Matrix::Multiply(defaultSkyViewProj, skyView, proj);

    const DrawParams params
    {
        rasterPass->GetSkyRasterPipelines(),
        // sky infos
        collectorSky->GetSkyDrawInfos(),
        rasterPass->GetSkyRasterRenderPass(),
        // sky FB
        rasterPass->GetSkyFramebuffer(frameIndex),
        rasterPass->GetRasterWidth(),
        rasterPass->GetRasterHeight(),
        // sky geometry
        collectorSky->GetVertexBuffer(),
        collectorSky->GetIndexBuffer(),
        textureManager->GetDescSet(frameIndex),
        defaultSkyViewProj
    };

    Draw(cmd, params);
}

void Rasterizer::DrawToFinalImage(VkCommandBuffer cmd, uint32_t frameIndex, 
                                  const std::shared_ptr<TextureManager> &textureManager, 
                                  float *view, float *proj,
                                  bool werePrimaryTraced)
{
    CmdLabel label(cmd, "Rasterized to final framebuf");


    typedef FramebufferImageIndex FI;
    FI fs[] =
    {
        FI::FB_IMAGE_INDEX_DEPTH,
        FI::FB_IMAGE_INDEX_FINAL
    };
    storageFramebuffers->BarrierMultiple(cmd, frameIndex, fs);


    // copy depth buffer
    rasterPass->PrepareForFinal(cmd, frameIndex, storageFramebuffers, werePrimaryTraced);

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    const DrawParams params =
    {
        rasterPass->GetRasterPipelines(),
        // ordinary infos
        collectorGeneral->GetRasterDrawInfos(),
        rasterPass->GetRasterRenderPass(),
        // ordinary FB
        rasterPass->GetFramebuffer(frameIndex),
        rasterPass->GetRasterWidth(),
        rasterPass->GetRasterHeight(),
        // ordinary geometry
        collectorGeneral->GetVertexBuffer(),
        collectorGeneral->GetIndexBuffer(),
        textureManager->GetDescSet(frameIndex),
        defaultViewProj
    };

    Draw(cmd, params);
}

void Rasterizer::DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainIndex, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj)
{
    CmdLabel label(cmd, "Rasterized to swapchain");

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    const DrawParams params = 
    {
        swapchainPass->GetSwapchainPipelines(),
        collectorGeneral->GetSwapchainDrawInfos(),
        swapchainPass->GetSwapchainRenderPass(),
        swapchainPass->GetSwapchainFramebuffer(swapchainIndex),
        swapchainPass->GetSwapchainWidth(),
        swapchainPass->GetSwapchainHeight(),
        collectorGeneral->GetVertexBuffer(),
        collectorGeneral->GetIndexBuffer(),
        textureManager->GetDescSet(frameIndex),
        defaultViewProj
    };

    Draw(cmd, params);
}

void Rasterizer::Draw(VkCommandBuffer cmd, const DrawParams &drawParams)
{
    assert(drawParams.framebuffer != VK_NULL_HANDLE);

    if (drawParams.drawInfos.empty())
    {
        return;
    }

    const VkViewport defaultViewport = { 0, 0, (float)drawParams.width, (float)drawParams.height, 0.0f, 1.0f };
    const VkRect2D defaultRenderArea = { { 0, 0 }, { drawParams.width, drawParams.height }};

    VkClearValue clear[2] = {};
    clear[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = drawParams.renderPass;
    beginInfo.framebuffer = drawParams.framebuffer;
    beginInfo.renderArea = defaultRenderArea;
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clear;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);


    VkPipeline curPipeline = VK_NULL_HANDLE;
    BindPipelineIfNew(cmd, drawParams.drawInfos[0], drawParams.pipelines, curPipeline);


    VkDeviceSize offset = 0;

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawParams.pipelines->GetPipelineLayout(), 0,
        1, &drawParams.descSet,
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
    pipelines->BindPipelineIfNew(cmd, curPipeline, info.blendEnable, info.blendFuncSrc, info.blendFuncDst, info.depthTest, info.depthWrite);
}

const std::shared_ptr<RenderCubemap> &Rasterizer::GetRenderCubemap() const
{
    return renderCubemap;
}

void Rasterizer::OnSwapchainCreate(const Swapchain *pSwapchain)
{
    swapchainPass->CreateFramebuffers(
        pSwapchain->GetWidth(), pSwapchain->GetHeight(),
        pSwapchain->GetImageViews(), pSwapchain->GetImageCount());
}

void Rasterizer::OnSwapchainDestroy()
{
    swapchainPass->DestroyFramebuffers();
}

void Rasterizer::OnShaderReload(const ShaderManager *shaderManager)
{
    rasterPass->OnShaderReload(shaderManager);
    swapchainPass->OnShaderReload(shaderManager);
    renderCubemap->OnShaderReload(shaderManager);
}

void Rasterizer::OnFramebuffersSizeChange(uint32_t width, uint32_t height)
{
    rasterPass->DestroyFramebuffers();
    rasterPass->CreateFramebuffers(width, height, storageFramebuffers, allocator, cmdManager);
}

void Rasterizer::CreatePipelineLayout(VkDescriptorSetLayout texturesSetLayout)
{
    VkPushConstantRange pushConst = {};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConst.offset = 0;
    pushConst.size = 16 * sizeof(float) + 4 * sizeof(float) + sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConst;

    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &texturesSetLayout;

    VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &commonPipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, commonPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Rasterizer common pipeline layout");
}

}
