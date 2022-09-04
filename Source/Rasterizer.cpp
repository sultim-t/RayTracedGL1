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

#include "Swapchain.h"
#include "Matrix.h"
#include "Utils.h"
#include "CmdLabel.h"
#include "RenderResolutionHelper.h"


namespace RTGL1
{



struct RasterizedPushConst
{
    float vp[16];
    float c[4];
    uint32_t t;
    uint32_t e;
    float emult;

    explicit RasterizedPushConst(const RasterizedDataCollector::DrawInfo &info, const float *defaultViewProj, float emissionMult)
    {
        float model[16];
        Matrix::ToMat4Transposed(model, info.transform);

        if (info.viewProj)
        {
            Matrix::Multiply(vp, model, info.viewProj->Get());
        }
        else
        {
            Matrix::Multiply(vp, model, defaultViewProj);
        }

        memcpy(c, info.color.Get(), 4 * sizeof(float));
        t = info.textureIndex;
        e = info.emissionTextureIndex;
        emult = emissionMult;
    }
};

static_assert(offsetof(RasterizedPushConst, vp) == 0);
static_assert(offsetof(RasterizedPushConst, c) == 64);
static_assert(offsetof(RasterizedPushConst, t) == 80);
static_assert(offsetof(RasterizedPushConst, e) == 84);
static_assert(offsetof(RasterizedPushConst, emult) == 88);
static_assert(sizeof(RasterizedPushConst) == 92);



Rasterizer::Rasterizer( VkDevice                                 _device,
                        VkPhysicalDevice                         _physDevice,
                        const std::shared_ptr< ShaderManager >&  _shaderManager,
                        const std::shared_ptr< TextureManager >& _textureManager,
                        const std::shared_ptr< GlobalUniform >&  _uniform,
                        const std::shared_ptr< SamplerManager >& _samplerManager,
                        const std::shared_ptr< Tonemapping >&    _tonemapping,
                        std::shared_ptr< MemoryAllocator >       _allocator,
                        std::shared_ptr< Framebuffers >          _storageFramebuffers,
                        std::shared_ptr< CommandBufferManager >  _cmdManager,
                        const RgInstanceCreateInfo&              _instanceInfo )
    : device( _device )
    , rasterPassPipelineLayout( VK_NULL_HANDLE )
    , swapchainPassPipelineLayout( VK_NULL_HANDLE )
    , allocator( std::move( _allocator ) )
    , cmdManager( std::move( _cmdManager ) )
    , storageFramebuffers( std::move( _storageFramebuffers ) )
{
    collector = std::make_shared<RasterizedDataCollector>(device, allocator, _textureManager, _instanceInfo.rasterizedMaxVertexCount, _instanceInfo.rasterizedMaxIndexCount);

    CreatePipelineLayout( _textureManager->GetDescSetLayout(), _tonemapping->GetDescSetLayout() );

    rasterPass = std::make_shared<RasterPass>(device, _physDevice, rasterPassPipelineLayout, _shaderManager, storageFramebuffers, _instanceInfo);
    swapchainPass = std::make_shared<SwapchainPass>(device, swapchainPassPipelineLayout, _shaderManager, _instanceInfo);
    renderCubemap = std::make_shared<RenderCubemap>(device, allocator, _shaderManager, _textureManager, _uniform, _samplerManager, cmdManager, _instanceInfo);

    lensFlares = std::make_unique<LensFlares>(device, allocator, _shaderManager, rasterPass->GetRasterRenderPass(), _uniform, storageFramebuffers, _textureManager, _instanceInfo);
}

Rasterizer::~Rasterizer()
{
    vkDestroyPipelineLayout( device, rasterPassPipelineLayout, nullptr );
    vkDestroyPipelineLayout( device, swapchainPassPipelineLayout, nullptr );
}

void Rasterizer::PrepareForFrame(uint32_t frameIndex)
{
    collector->Clear(frameIndex);
    lensFlares->PrepareForFrame(frameIndex);
}

void Rasterizer::Upload(uint32_t frameIndex, 
                        const RgRasterizedGeometryUploadInfo &uploadInfo, 
                        const float *viewProjection, const RgViewport *viewport)
{
    collector->AddGeometry(frameIndex, uploadInfo, viewProjection, viewport);
}

void Rasterizer::UploadLensFlare(uint32_t frameIndex, const RgLensFlareUploadInfo &uploadInfo)
{
    lensFlares->Upload(frameIndex, uploadInfo);
}

void Rasterizer::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    CmdLabel label(cmd, "Copying rasterizer data");

    collector->CopyFromStaging(cmd, frameIndex);
    lensFlares->SubmitForFrame(cmd, frameIndex);
}

void Rasterizer::DrawSkyToCubemap(VkCommandBuffer cmd, uint32_t frameIndex, 
                                  const std::shared_ptr<TextureManager> &textureManager, 
                                  const std::shared_ptr<GlobalUniform> &uniform)
{
    CmdLabel label(cmd, "Rasterized sky to cubemap");

    renderCubemap->Draw(cmd, frameIndex, collector, textureManager, uniform);
}

namespace
{
    void ApplyJitter(float *jitterredProj, const float *originalProj, const RgFloat2D &jitter, const RenderResolutionHelper &renderResolution)
    {
        memcpy(jitterredProj, originalProj, 16 * sizeof(float));
        jitterredProj[2 * 4 + 0] += jitter.data[0] / (float)renderResolution.Width();
        jitterredProj[2 * 4 + 1] += jitter.data[1] / (float)renderResolution.Height();
    }
}

void Rasterizer::DrawSkyToAlbedo(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<TextureManager> &textureManager, 
                                 const float *view, const float skyViewerPos[3], const float *proj,
                                 const RgFloat2D &jitter, const RenderResolutionHelper &renderResolution)
{
    CmdLabel label(cmd, "Rasterized sky to albedo framebuf");


    typedef FramebufferImageIndex FI;
    storageFramebuffers->BarrierOne(cmd, frameIndex, FI::FB_IMAGE_INDEX_ALBEDO);


    float skyView[16];
    Matrix::SetNewViewerPosition(skyView, view, skyViewerPos);

    float jitterredProj[16];
    ApplyJitter(jitterredProj, proj, jitter, renderResolution);

    float defaultSkyViewProj[16];
    Matrix::Multiply(defaultSkyViewProj, skyView, jitterredProj);


    VkDescriptorSet sets[] = {
        textureManager->GetDescSet( frameIndex ),
    };

    const DrawParams params = {
        .pipelines       = rasterPass->GetSkyRasterPipelines(),
        .drawInfos       = collector->GetSkyDrawInfos(),
        .renderPass      = rasterPass->GetSkyRasterRenderPass(),
        .framebuffer     = rasterPass->GetSkyFramebuffer( frameIndex ),
        .width           = rasterPass->GetRasterWidth(),
        .height          = rasterPass->GetRasterHeight(),
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultSkyViewProj,
        .pLensFlares     = nullptr,
        .emissionMult    = std::nullopt,
    };

    Draw(cmd, frameIndex, params);
}

void Rasterizer::DrawToFinalImage( VkCommandBuffer                          cmd,
                                   uint32_t                                 frameIndex,
                                   const std::shared_ptr< TextureManager >& textureManager,
                                   const std::shared_ptr< Tonemapping >&    tonemapping,
                                   const float*                             view,
                                   const float*                             proj,
                                   const RgFloat2D&                         jitter,
                                   const RenderResolutionHelper&            renderResolution,
                                   const RgDrawFrameLensFlareParams*        pLensFlareParams,
                                   float                                    emissionMult )
{
    CmdLabel label(cmd, "Rasterized to final framebuf");


    typedef FramebufferImageIndex FI;
    FI fs[] =
    {
        FI::FB_IMAGE_INDEX_DEPTH_NDC,
        FI::FB_IMAGE_INDEX_FINAL
    };
    storageFramebuffers->BarrierMultiple(cmd, frameIndex, fs);


    // prepare lens flares draw commands
    lensFlares->SetParams(pLensFlareParams);
    lensFlares->Cull(cmd, frameIndex);


    // copy depth buffer
    rasterPass->PrepareForFinal(cmd, frameIndex, storageFramebuffers);


    float jitterredProj[16];
    ApplyJitter(jitterredProj, proj, jitter, renderResolution);


    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, jitterredProj);

    VkDescriptorSet sets[] = {
         textureManager->GetDescSet( frameIndex ),
         tonemapping->GetDescSet(),
    };

    const DrawParams params = {
        .pipelines       = rasterPass->GetRasterPipelines(),
        .drawInfos       = collector->GetRasterDrawInfos(),
        .renderPass      = rasterPass->GetRasterRenderPass(),
        .framebuffer     = rasterPass->GetFramebuffer( frameIndex ),
        .width           = rasterPass->GetRasterWidth(),
        .height          = rasterPass->GetRasterHeight(),
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultViewProj,
        .pLensFlares     = lensFlares.get(),
        .emissionMult    = emissionMult,
    };

    Draw(cmd, frameIndex, params);
}

void Rasterizer::DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, FramebufferImageIndex imageToDrawIn, const std::shared_ptr<TextureManager> &textureManager, float *view, float *proj)
{
    CmdLabel label(cmd, "Rasterized to swapchain");


    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);


    VkDescriptorSet sets[] = {
        textureManager->GetDescSet( frameIndex ),
    };

    const DrawParams params = {
        .pipelines       = swapchainPass->GetSwapchainPipelines(),
        .drawInfos       = collector->GetSwapchainDrawInfos(),
        .renderPass      = swapchainPass->GetSwapchainRenderPass(),
        .framebuffer     = swapchainPass->GetSwapchainFramebuffer( imageToDrawIn, frameIndex ),
        .width           = swapchainPass->GetSwapchainWidth(),
        .height          = swapchainPass->GetSwapchainHeight(),
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultViewProj,
        .pLensFlares     = nullptr,
        .emissionMult    = std::nullopt,
    };

    Draw(cmd, frameIndex, params);
}

void Rasterizer::Draw(VkCommandBuffer cmd, uint32_t frameIndex, const DrawParams &drawParams)
{
    assert(drawParams.framebuffer != VK_NULL_HANDLE);

    const bool draw = !drawParams.drawInfos.empty();
    const bool drawLensFlares = drawParams.pLensFlares != nullptr && drawParams.pLensFlares->GetCullingInputCount() > 0;

    if (!draw && !drawLensFlares)
    {
        return;
    }


    if (drawLensFlares)
    {
        drawParams.pLensFlares->SyncForDraw(cmd, frameIndex);
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


    if (draw)
    {
        VkPipeline curPipeline = VK_NULL_HANDLE;
        BindPipelineIfNew(cmd, drawParams.drawInfos[0], drawParams.pipelines, curPipeline);


        VkDeviceSize offset = 0;

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawParams.pipelines->GetPipelineLayout(), 0,
            drawParams.descSetsCount, drawParams.descSets,
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
                RasterizedPushConst push(info, drawParams.defaultViewProj, drawParams.emissionMult.value_or(0.0f));

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
    }


    if (drawLensFlares)
    {
        vkCmdSetScissor(cmd, 0, 1, &defaultRenderArea);
        vkCmdSetViewport(cmd, 0, 1, &defaultViewport);

        drawParams.pLensFlares->Draw(cmd, frameIndex);
    }


    vkCmdEndRenderPass(cmd);
}

void Rasterizer::SetViewportIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, 
                                  const VkViewport &defaultViewport, VkViewport &curViewport)
{
    const VkViewport& newViewport = info.viewport.value_or( defaultViewport );

    if( !Utils::AreViewportsSame( curViewport, newViewport ) )
    {
        vkCmdSetViewport( cmd, 0, 1, &newViewport );
        curViewport = newViewport;
    }
}

void Rasterizer::BindPipelineIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info,
    const std::shared_ptr<RasterizerPipelines> &pipelines, VkPipeline &curPipeline)
{
    pipelines->BindPipelineIfNew(cmd, curPipeline, info.pipelineState, info.blendFuncSrc, info.blendFuncDst);
}

const std::shared_ptr<RenderCubemap> &Rasterizer::GetRenderCubemap() const
{
    return renderCubemap;
}

uint32_t Rasterizer::GetLensFlareCullingInputCount() const
{
    return lensFlares->GetCullingInputCount();
}

void Rasterizer::OnShaderReload(const ShaderManager *shaderManager)
{
    rasterPass->OnShaderReload(shaderManager);
    swapchainPass->OnShaderReload(shaderManager);
    renderCubemap->OnShaderReload(shaderManager);
    lensFlares->OnShaderReload(shaderManager);
}

void Rasterizer::OnFramebuffersSizeChange(const ResolutionState &resolutionState)
{
    rasterPass->DestroyFramebuffers();
    swapchainPass->DestroyFramebuffers();

    rasterPass->CreateFramebuffers(resolutionState.renderWidth, resolutionState.renderHeight, storageFramebuffers, allocator, cmdManager);
    swapchainPass->CreateFramebuffers(resolutionState.upscaledWidth, resolutionState.upscaledHeight, storageFramebuffers);
}

void Rasterizer::CreatePipelineLayout( VkDescriptorSetLayout texturesSetLayout,
                                       VkDescriptorSetLayout tonemappingSetLayout )
{
    const VkPushConstantRange pushConst = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof( RasterizedPushConst ),
    };

    {
        VkDescriptorSetLayout layouts[] = {
            texturesSetLayout,
            tonemappingSetLayout,
        };

        VkPipelineLayoutCreateInfo layoutInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = std::size( layouts ),
            .pSetLayouts            = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConst,
        };

        VkResult r =
            vkCreatePipelineLayout( device, &layoutInfo, nullptr, &rasterPassPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        rasterPassPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Raster pass Pipeline layout" );
    }

    {
        VkPipelineLayoutCreateInfo layoutInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &texturesSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConst,
        };

        VkResult r =
            vkCreatePipelineLayout( device, &layoutInfo, nullptr, &swapchainPassPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        swapchainPassPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Swapchain pass Pipeline layout" );
    }
}

}
