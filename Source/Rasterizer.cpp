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

namespace
{

struct RasterizedPushConst
{
    float    vp[ 16 ];
    uint32_t packedColor;
    uint32_t textureIndex;
    uint32_t emissionTextureIndex;

    explicit RasterizedPushConst( const RTGL1::RasterizedDataCollector::DrawInfo& info,
                                  const float*                                    defaultViewProj )
        : vp{}
        , packedColor{ info.base_color }
        , textureIndex( info.base_textureA )
        , emissionTextureIndex( info.base_textureB )
    {
        float model[ 16 ] = RG_MATRIX_TRANSPOSED( info.transform );
        RTGL1::Matrix::Multiply(
            vp, model, info.viewProj ? info.viewProj->Get() : defaultViewProj );
    }
};

static_assert( offsetof( RasterizedPushConst, vp ) == 0 );
static_assert( offsetof( RasterizedPushConst, packedColor ) == 64 );
static_assert( offsetof( RasterizedPushConst, textureIndex ) == 68 );
static_assert( offsetof( RasterizedPushConst, emissionTextureIndex ) == 72 );
static_assert( sizeof( RasterizedPushConst ) == 76 );

}

RTGL1::Rasterizer::Rasterizer( VkDevice                                _device,
                               VkPhysicalDevice                        _physDevice,
                               const ShaderManager&                    _shaderManager,
                               std::shared_ptr< TextureManager >       _textureManager,
                               const GlobalUniform&                    _uniform,
                               const SamplerManager&                   _samplerManager,
                               const Tonemapping&                      _tonemapping,
                               const Volumetric&                       _volumetric,
                               std::shared_ptr< MemoryAllocator >      _allocator,
                               std::shared_ptr< Framebuffers >         _storageFramebuffers,
                               std::shared_ptr< CommandBufferManager > _cmdManager,
                               const RgInstanceCreateInfo&             _instanceInfo )
    : device( _device )
    , rasterPassPipelineLayout( VK_NULL_HANDLE )
    , swapchainPassPipelineLayout( VK_NULL_HANDLE )
    , allocator( std::move( _allocator ) )
    , cmdManager( std::move( _cmdManager ) )
    , storageFramebuffers( std::move( _storageFramebuffers ) )
{
    collector =
        std::make_shared< RasterizedDataCollector >( device,
                                                     allocator,
                                                     _textureManager,
                                                     _instanceInfo.rasterizedMaxVertexCount,
                                                     _instanceInfo.rasterizedMaxIndexCount );

    VkDescriptorSetLayout layouts[] = {
        _textureManager->GetDescSetLayout(),
        _uniform.GetDescSetLayout(),
        _tonemapping.GetDescSetLayout(),
        _volumetric.GetDescSetLayout(),
    };
    CreatePipelineLayouts( layouts, std::size( layouts ), _textureManager->GetDescSetLayout() );

    rasterPass = std::make_shared< RasterPass >( device,
                                                 _physDevice,
                                                 rasterPassPipelineLayout,
                                                 _shaderManager,
                                                 *storageFramebuffers,
                                                 _instanceInfo );

    swapchainPass = std::make_shared< SwapchainPass >(
        device, swapchainPassPipelineLayout, _shaderManager, _instanceInfo );

    renderCubemap = std::make_shared< RenderCubemap >( device,
                                                       *allocator,
                                                       _shaderManager,
                                                       *_textureManager,
                                                       _uniform,
                                                       _samplerManager,
                                                       *cmdManager,
                                                       _instanceInfo );

    /*
    lensFlares = std::make_unique< LensFlares >( device,
                                                 allocator,
                                                 _shaderManager,
                                                 rasterPass->GetWorldRenderPass(),
                                                 _uniform,
                                                 storageFramebuffers,
                                                 _textureManager,
                                                 _instanceInfo );
    */
}

RTGL1::Rasterizer::~Rasterizer()
{
    vkDestroyPipelineLayout( device, rasterPassPipelineLayout, nullptr );
    vkDestroyPipelineLayout( device, swapchainPassPipelineLayout, nullptr );
}

void RTGL1::Rasterizer::PrepareForFrame( uint32_t frameIndex )
{
    collector->Clear( frameIndex );
    // lensFlares->PrepareForFrame( frameIndex );
}

void RTGL1::Rasterizer::Upload( uint32_t                   frameIndex,
                                GeometryRasterType         rasterType,
                                const RgTransform&         transform,
                                const RgMeshPrimitiveInfo& info,
                                const float*               pViewProjection,
                                const RgViewport*          pViewport )
{
    collector->AddPrimitive( frameIndex, rasterType, transform, info, pViewProjection, pViewport );
}

void RTGL1::Rasterizer::SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    CmdLabel label( cmd, "Copying rasterizer data" );

    collector->CopyFromStaging( cmd, frameIndex );
}

void RTGL1::Rasterizer::DrawSkyToCubemap( VkCommandBuffer       cmd,
                                          uint32_t              frameIndex,
                                          const TextureManager& textureManager,
                                          const GlobalUniform&  uniform )
{
    CmdLabel label( cmd, "Rasterized sky to cubemap" );

    renderCubemap->Draw( cmd, frameIndex, *collector, textureManager, uniform );
}

namespace RTGL1
{
namespace
{
    void ApplyJitter( float*                               jitterredProj,
                      const float*                         originalProj,
                      const RgFloat2D&                     jitter,
                      const RTGL1::RenderResolutionHelper& renderResolution )
    {
        memcpy( jitterredProj, originalProj, 16 * sizeof( float ) );
        jitterredProj[ 2 * 4 + 0 ] +=
            jitter.data[ 0 ] / static_cast< float >( renderResolution.Width() );
        jitterredProj[ 2 * 4 + 1 ] +=
            jitter.data[ 1 ] / static_cast< float >( renderResolution.Height() );
    }


    void SetViewportIfNew( VkCommandBuffer                          cmd,
                           const RasterizedDataCollector::DrawInfo& info,
                           const VkViewport&                        defaultViewport,
                           VkViewport&                              curViewport )
    {
        const VkViewport newViewport = info.viewport.value_or( defaultViewport );

        if( !Utils::AreViewportsSame( curViewport, newViewport ) )
        {
            vkCmdSetViewport( cmd, 0, 1, &newViewport );
            curViewport = newViewport;
        }
    }
}
}

namespace RTGL1
{

struct RasterDrawParams
{
    const std::shared_ptr< RTGL1::RasterizerPipelines >&           pipelines;
    const std::vector< RTGL1::RasterizedDataCollector::DrawInfo >& drawInfos;
    VkRenderPass                                                   renderPass;
    VkFramebuffer                                                  framebuffer;
    uint32_t                                                       width;
    uint32_t                                                       height;
    VkBuffer                                                       vertexBuffer;
    VkBuffer                                                       indexBuffer;
    const VkDescriptorSet*                                         descSets;
    uint32_t                                                       descSetsCount;
    float*                                                         defaultViewProj;
};

}

void RTGL1::Rasterizer::DrawSkyToAlbedo( VkCommandBuffer               cmd,
                                         uint32_t                      frameIndex,
                                         const TextureManager&         textureManager,
                                         const float*                  view,
                                         const float                   skyViewerPos[ 3 ],
                                         const float*                  proj,
                                         const RgFloat2D&              jitter,
                                         const RenderResolutionHelper& renderResolution )
{
    CmdLabel label( cmd, "Rasterized sky to albedo framebuf" );


    using FI = FramebufferImageIndex;
    storageFramebuffers->BarrierOne( cmd, frameIndex, FI::FB_IMAGE_INDEX_ALBEDO );


    float skyView[ 16 ];
    Matrix::SetNewViewerPosition( skyView, view, skyViewerPos );

    float jitterredProj[ 16 ];
    ApplyJitter( jitterredProj, proj, jitter, renderResolution );

    float defaultSkyViewProj[ 16 ];
    Matrix::Multiply( defaultSkyViewProj, skyView, jitterredProj );


    VkDescriptorSet sets[] = {
        textureManager.GetDescSet( frameIndex ),
    };

    const RasterDrawParams params = {
        .pipelines       = rasterPass->GetSkyRasterPipelines(),
        .drawInfos       = collector->GetSkyDrawInfos(),
        .renderPass      = rasterPass->GetSkyRenderPass(),
        .framebuffer     = rasterPass->GetSkyFramebuffer( frameIndex ),
        .width           = renderResolution.Width(),
        .height          = renderResolution.Height(),
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultSkyViewProj,
    };

    Draw( cmd, frameIndex, params );
}

void RTGL1::Rasterizer::DrawToFinalImage( VkCommandBuffer               cmd,
                                          uint32_t                      frameIndex,
                                          const TextureManager&         textureManager,
                                          const GlobalUniform&          uniform,
                                          const Tonemapping&            tonemapping,
                                          const Volumetric&             volumetric,
                                          const float*                  view,
                                          const float*                  proj,
                                          const RgFloat2D&              jitter,
                                          const RenderResolutionHelper& renderResolution )
{
    CmdLabel label( cmd, "Rasterized to final framebuf" );
    using FI = FramebufferImageIndex;


    FI fs[] = {
        FI::FB_IMAGE_INDEX_DEPTH_NDC,
        FI::FB_IMAGE_INDEX_FINAL,
    };
    storageFramebuffers->BarrierMultiple( cmd, frameIndex, fs );


    // copy depth buffer
    rasterPass->PrepareForFinal( cmd,
                                 frameIndex,
                                 *storageFramebuffers,
                                 renderResolution.Width(),
                                 renderResolution.Height() );


    float jitterredProj[ 16 ];
    ApplyJitter( jitterredProj, proj, jitter, renderResolution );


    float defaultViewProj[ 16 ];
    Matrix::Multiply( defaultViewProj, view, jitterredProj );

    VkDescriptorSet sets[] = {
        textureManager.GetDescSet( frameIndex ),
        uniform.GetDescSet( frameIndex ),
        tonemapping.GetDescSet(),
        volumetric.GetDescSet( frameIndex ),
    };

    const RasterDrawParams params = {
        .pipelines       = rasterPass->GetRasterPipelines(),
        .drawInfos       = collector->GetRasterDrawInfos(),
        .renderPass      = rasterPass->GetWorldRenderPass(),
        .framebuffer     = rasterPass->GetWorldFramebuffer( frameIndex ),
        .width           = renderResolution.Width(),
        .height          = renderResolution.Height(),
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultViewProj,
    };

    Draw( cmd, frameIndex, params );
}

void RTGL1::Rasterizer::DrawToSwapchain( VkCommandBuffer       cmd,
                                         uint32_t              frameIndex,
                                         FramebufferImageIndex imageToDrawIn,
                                         const TextureManager& textureManager,
                                         const float*          view,
                                         const float*          proj,
                                         uint32_t              swapchainWidth,
                                         uint32_t              swapchainHeight )
{
    CmdLabel label( cmd, "Rasterized to swapchain" );


    float    defaultViewProj[ 16 ];
    Matrix::Multiply( defaultViewProj, view, proj );


    VkDescriptorSet sets[] = {
        textureManager.GetDescSet( frameIndex ),
    };

    const RasterDrawParams params = {
        .pipelines       = swapchainPass->GetSwapchainPipelines(),
        .drawInfos       = collector->GetSwapchainDrawInfos(),
        .renderPass      = swapchainPass->GetSwapchainRenderPass(),
        .framebuffer     = swapchainPass->GetSwapchainFramebuffer( imageToDrawIn, frameIndex ),
        .width           = swapchainWidth,
        .height          = swapchainHeight,
        .vertexBuffer    = collector->GetVertexBuffer(),
        .indexBuffer     = collector->GetIndexBuffer(),
        .descSets        = sets,
        .descSetsCount   = std::size( sets ),
        .defaultViewProj = defaultViewProj,
    };

    Draw( cmd, frameIndex, params );
}

void RTGL1::Rasterizer::Draw( VkCommandBuffer         cmd,
                              uint32_t                frameIndex,
                              const RasterDrawParams& drawParams )
{
    assert( drawParams.framebuffer != VK_NULL_HANDLE );
    if( drawParams.drawInfos.empty() )
    {
        return;
    }

    const VkViewport defaultViewport = {
        .x        = 0,
        .y        = 0,
        .width    = static_cast< float >( drawParams.width ),
        .height   = static_cast< float >( drawParams.height ),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D defaultRenderArea = {
        .offset = { 0, 0 },
        .extent = { drawParams.width, drawParams.height },
    };

    const VkClearValue clear[] = {
        {
            .color = { .float32 = { 0.0f, 0.0f, 0.0f, 0.0 } },
        },
        {
            .depthStencil = { .depth = 1.0f },
        },
    };

    VkRenderPassBeginInfo beginInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = drawParams.renderPass,
        .framebuffer     = drawParams.framebuffer,
        .renderArea      = defaultRenderArea,
        .clearValueCount = std::size( clear ),
        .pClearValues    = clear,
    };

    vkCmdBeginRenderPass( cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );


    VkPipeline curPipeline = drawParams.pipelines->BindPipelineIfNew(
        cmd, VK_NULL_HANDLE, drawParams.drawInfos[ 0 ].pipelineState );

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             drawParams.pipelines->GetPipelineLayout(),
                             0,
                             drawParams.descSetsCount,
                             drawParams.descSets,
                             0,
                             nullptr );

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers( cmd, 0, 1, &drawParams.vertexBuffer, &offset );
    vkCmdBindIndexBuffer( cmd, drawParams.indexBuffer, offset, VK_INDEX_TYPE_UINT32 );


    vkCmdSetScissor( cmd, 0, 1, &defaultRenderArea );
    vkCmdSetViewport( cmd, 0, 1, &defaultViewport );
    VkViewport curViewport = defaultViewport;


    for( const auto& info : drawParams.drawInfos )
    {
        SetViewportIfNew( cmd, info, defaultViewport, curViewport );
        curPipeline =
            drawParams.pipelines->BindPipelineIfNew( cmd, curPipeline, info.pipelineState );

        // push const
        {
            RasterizedPushConst push( info, drawParams.defaultViewProj );

            vkCmdPushConstants( cmd,
                                drawParams.pipelines->GetPipelineLayout(),
                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                0,
                                sizeof( push ),
                                &push );
        }

        // draw
        if( info.indexCount > 0 )
        {
            vkCmdDrawIndexed( cmd, info.indexCount, 1, info.firstIndex, info.firstVertex, 0 );
        }
        else
        {
            vkCmdDraw( cmd, info.vertexCount, 1, info.firstVertex, 0 );
        }
    }


    vkCmdEndRenderPass( cmd );
}

const std::shared_ptr< RTGL1::RenderCubemap >& RTGL1::Rasterizer::GetRenderCubemap() const
{
    return renderCubemap;
}

void RTGL1::Rasterizer::OnShaderReload( const ShaderManager* shaderManager )
{
    rasterPass->OnShaderReload( shaderManager );
    swapchainPass->OnShaderReload( shaderManager );
    renderCubemap->OnShaderReload( shaderManager );
}

void RTGL1::Rasterizer::OnFramebuffersSizeChange( const ResolutionState& resolutionState )
{
    rasterPass->DestroyFramebuffers();
    swapchainPass->DestroyFramebuffers();

    rasterPass->CreateFramebuffers( resolutionState.renderWidth,
                                    resolutionState.renderHeight,
                                    *storageFramebuffers,
                                    *allocator,
                                    *cmdManager );
    swapchainPass->CreateFramebuffers(
        resolutionState.upscaledWidth, resolutionState.upscaledHeight, storageFramebuffers );
}

void RTGL1::Rasterizer::CreatePipelineLayouts( VkDescriptorSetLayout* allLayouts,
                                               size_t                 count,
                                               VkDescriptorSetLayout  texturesSetLayout )
{
    const VkPushConstantRange pushConst = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof( RasterizedPushConst ),
    };

    {
        VkPipelineLayoutCreateInfo layoutInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast< uint32_t >( count ),
            .pSetLayouts            = allLayouts,
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
