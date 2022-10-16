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


constexpr VkFormat    DEPTH_FORMAT      = VK_FORMAT_D32_SFLOAT;
constexpr const char* DEPTH_FORMAT_NAME = "VK_FORMAT_D32_SFLOAT";


RTGL1::RasterPass::RasterPass( VkDevice                    _device,
                               VkPhysicalDevice            _physDevice,
                               VkPipelineLayout            _pipelineLayout,
                               const ShaderManager&        _shaderManager,
                               const Framebuffers&         _storageFramebuffers,
                               const RgInstanceCreateInfo& _instanceInfo )
    : device( _device )
{
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties( _physDevice, DEPTH_FORMAT, &props );
        if( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) == 0 )
        {
            using namespace std::string_literals;
            throw RgException( RG_RESULT_GRAPHICS_API_ERROR,
                               "Depth format is not supported: "s + DEPTH_FORMAT_NAME );
        }
    }

    worldRenderPass =
        CreateWorldRenderPass( ShFramebuffers_Formats[ FB_IMAGE_INDEX_FINAL ],
                               ShFramebuffers_Formats[ FB_IMAGE_INDEX_SCREEN_EMISSION ],
                               DEPTH_FORMAT );

    skyRenderPass =
        CreateSkyRenderPass( ShFramebuffers_Formats[ FB_IMAGE_INDEX_ALBEDO ], DEPTH_FORMAT );

    worldPipelines =
        std::make_shared< RasterizerPipelines >( device,
                                                 _pipelineLayout,
                                                 worldRenderPass,
                                                 &_shaderManager,
                                                 "VertDefault",
                                                 "FragWorld",
                                                 1 /* for emission */,
                                                 _instanceInfo.rasterizedVertexColorGamma );

    skyPipelines =
        std::make_shared< RasterizerPipelines >( device,
                                                 _pipelineLayout,
                                                 skyRenderPass,
                                                 &_shaderManager,
                                                 "VertDefault",
                                                 "FragSky",
                                                 0,
                                                 _instanceInfo.rasterizedVertexColorGamma );

    depthCopying = std::make_shared< DepthCopying >(
        device, DEPTH_FORMAT, _shaderManager, _storageFramebuffers );
}

RTGL1::RasterPass::~RasterPass()
{
    vkDestroyRenderPass( device, worldRenderPass, nullptr );
    vkDestroyRenderPass( device, skyRenderPass, nullptr );
    DestroyFramebuffers();
}

void RTGL1::RasterPass::PrepareForFinal( VkCommandBuffer     cmd,
                                         uint32_t            frameIndex,
                                         const Framebuffers& storageFramebuffers,
                                         uint32_t            renderWidth,
                                         uint32_t            renderHeight )
{
    // firstly, copy data from storage buffer to depth buffer,
    // and only after getting correct depth buffer, draw the geometry
    // if no primary rays were traced, just clear depth buffer without copying
    depthCopying->Process( cmd, frameIndex, storageFramebuffers, renderWidth, renderHeight, false );
}

void RTGL1::RasterPass::CreateFramebuffers( uint32_t              renderWidth,
                                            uint32_t              renderHeight,
                                            const Framebuffers&   storageFramebuffers,
                                            MemoryAllocator&      allocator,
                                            CommandBufferManager& cmdManager )
{
    CreateDepthBuffers( renderWidth, renderHeight, allocator, cmdManager );

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( worldFramebuffers[ i ] == VK_NULL_HANDLE );
        {
            VkImageView attchs[] = {
                storageFramebuffers.GetImageView( FB_IMAGE_INDEX_FINAL, i ),
                storageFramebuffers.GetImageView( FB_IMAGE_INDEX_SCREEN_EMISSION, i ),
                depthViews[ i ],
            };

            VkFramebufferCreateInfo fbInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = worldRenderPass,
                .attachmentCount = std::size( attchs ),
                .pAttachments    = attchs,
                .width           = renderWidth,
                .height          = renderHeight,
                .layers          = 1,
            };

            VkResult r = vkCreateFramebuffer( device, &fbInfo, nullptr, &worldFramebuffers[ i ] );
            VK_CHECKERROR( r );

            SET_DEBUG_NAME( device,
                            worldFramebuffers[ i ],
                            VK_OBJECT_TYPE_FRAMEBUFFER,
                            "Rasterizer raster framebuffer" );
        }

        assert( skyFramebuffers[ i ] == VK_NULL_HANDLE );
        {
            VkImageView attchs[] = {
                storageFramebuffers.GetImageView( FB_IMAGE_INDEX_ALBEDO, i ),
                depthViews[ i ],
            };

            VkFramebufferCreateInfo fbInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = skyRenderPass,
                .attachmentCount = std::size( attchs ),
                .pAttachments    = attchs,
                .width           = renderWidth,
                .height          = renderHeight,
                .layers          = 1,
            };

            VkResult r = vkCreateFramebuffer( device, &fbInfo, nullptr, &skyFramebuffers[ i ] );
            VK_CHECKERROR( r );

            SET_DEBUG_NAME( device,
                            skyFramebuffers[ i ],
                            VK_OBJECT_TYPE_FRAMEBUFFER,
                            "Rasterizer raster sky framebuffer" );
        }
    }

    depthCopying->CreateFramebuffers( depthViews, renderWidth, renderHeight );
}

void RTGL1::RasterPass::DestroyFramebuffers()
{
    depthCopying->DestroyFramebuffers();

    DestroyDepthBuffers();

    for( VkFramebuffer& fb : worldFramebuffers )
    {
        if( fb != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, fb, nullptr );
            fb = VK_NULL_HANDLE;
        }
    }

    for( VkFramebuffer& fb : skyFramebuffers )
    {
        if( fb != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, fb, nullptr );
            fb = VK_NULL_HANDLE;
        }
    }
}

VkRenderPass RTGL1::RasterPass::GetWorldRenderPass() const
{
    return worldRenderPass;
}

VkRenderPass RTGL1::RasterPass::GetSkyRenderPass() const
{
    return skyRenderPass;
}

const std::shared_ptr< RTGL1::RasterizerPipelines >& RTGL1::RasterPass::GetRasterPipelines() const
{
    return worldPipelines;
}

const std::shared_ptr< RTGL1::RasterizerPipelines >& RTGL1::RasterPass::GetSkyRasterPipelines()
    const
{
    return skyPipelines;
}

VkFramebuffer RTGL1::RasterPass::GetWorldFramebuffer( uint32_t frameIndex ) const
{
    return worldFramebuffers[ frameIndex ];
}

VkFramebuffer RTGL1::RasterPass::GetSkyFramebuffer( uint32_t frameIndex ) const
{
    return skyFramebuffers[ frameIndex ];
}

void RTGL1::RasterPass::OnShaderReload( const ShaderManager* shaderManager )
{
    worldPipelines->OnShaderReload( shaderManager );
    skyPipelines->OnShaderReload( shaderManager );

    depthCopying->OnShaderReload( shaderManager );
}

VkRenderPass RTGL1::RasterPass::CreateWorldRenderPass( VkFormat finalImageFormat,
                                                       VkFormat screenEmisionFormat,
                                                       VkFormat depthImageFormat ) const
{
    const VkAttachmentDescription attchs[] = {
        {
            // final image attachment
            .format         = finalImageFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
        },
        {
            // screen emission image attachment
            .format         = screenEmisionFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
        },
        {
            .format  = depthImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            // load depth data from depthCopying
            .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            // depth image was already transitioned
            // by depthCopying for rasterRenderPass
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference colorRefs[] = { {
                                              .attachment = 0,
                                              .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          },
                                          {
                                              .attachment = 1,
                                              .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          } };

    VkAttachmentReference depthRef = {
        .attachment = 2,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = std::size( colorRefs ),
        .pColorAttachments       = colorRefs,
        .pDepthStencilAttachment = &depthRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo passInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = std::size( attchs ),
        .pAttachments    = attchs,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkRenderPass pass;
    VkResult     r = vkCreateRenderPass( device, &passInfo, nullptr, &pass );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, pass, VK_OBJECT_TYPE_RENDER_PASS, "Rasterizer raster render pass" );

    return pass;
}

VkRenderPass RTGL1::RasterPass::CreateSkyRenderPass( VkFormat skyFinalImageFormat,
                                                     VkFormat depthImageFormat ) const
{
    const VkAttachmentDescription attchs[] = {
        {
            // sky attachment
            .format         = skyFinalImageFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
        },
        {
            .format  = depthImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            // clear for sky
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            // depth image was already transitioned
            // manually for rasterSkyRenderPass
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference colorRefs[] = { {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    } };

    VkAttachmentReference depthRef = {
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = std::size( colorRefs ),
        .pColorAttachments       = colorRefs,
        .pDepthStencilAttachment = &depthRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo passInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = std::size( attchs ),
        .pAttachments    = attchs,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkRenderPass pass;
    VkResult     r = vkCreateRenderPass( device, &passInfo, nullptr, &pass );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, pass, VK_OBJECT_TYPE_RENDER_PASS, "Rasterizer raster sky render pass" );

    return pass;
}

void RTGL1::RasterPass::CreateDepthBuffers( uint32_t              width,
                                            uint32_t              height,
                                            MemoryAllocator&      allocator,
                                            CommandBufferManager& cmdManager )
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( depthImages[ i ] == VK_NULL_HANDLE );
        assert( depthViews[ i ] == VK_NULL_HANDLE );
        assert( depthMemory[ i ] == VK_NULL_HANDLE );

        {
            VkImageCreateInfo imageInfo = {
                .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .flags         = 0,
                .imageType     = VK_IMAGE_TYPE_2D,
                .format        = DEPTH_FORMAT,
                .extent        = { width, height, 1 },
                .mipLevels     = 1,
                .arrayLayers   = 1,
                .samples       = VK_SAMPLE_COUNT_1_BIT,
                .tiling        = VK_IMAGE_TILING_OPTIMAL,
                .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            VkResult r = vkCreateImage( device, &imageInfo, nullptr, &depthImages[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            depthImages[ i ],
                            VK_OBJECT_TYPE_IMAGE,
                            "Rasterizer raster pass depth image" );
        }
        {
            VkMemoryRequirements memReqs;
            vkGetImageMemoryRequirements( device, depthImages[ i ], &memReqs );

            depthMemory[ i ] = allocator.AllocDedicated( memReqs,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                         MemoryAllocator::AllocType::DEFAULT,
                                                         "Rasterizer raster pass depth memory" );

            if( depthMemory[ i ] == VK_NULL_HANDLE )
            {
                vkDestroyImage( device, depthImages[ i ], nullptr );
                depthImages[ i ] = VK_NULL_HANDLE;

                return;
            }

            VkResult r = vkBindImageMemory( device, depthImages[ i ], depthMemory[ i ], 0 );
            VK_CHECKERROR( r );
        }
        {
            VkImageViewCreateInfo viewInfo = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image            = depthImages[ i ],
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = DEPTH_FORMAT,
                .subresourceRange = { .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                                      .baseMipLevel   = 0,
                                      .levelCount     = 1,
                                      .baseArrayLayer = 0,
                                      .layerCount     = 1 },
            };

            VkResult r = vkCreateImageView( device, &viewInfo, nullptr, &depthViews[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            depthViews[ i ],
                            VK_OBJECT_TYPE_IMAGE_VIEW,
                            "Rasterizer raster pass depth image view" );
        }

        // make transition from undefined manually,
        // so depthAttch.initialLayout can be specified as DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkCommandBuffer      cmd = cmdManager.StartGraphicsCmd();

        VkImageMemoryBarrier imageBarrier = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = depthImages[ i ],
            .subresourceRange    = { .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount     = 1 }
        };

        vkCmdPipelineBarrier( cmd,
                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                              0,
                              0,
                              nullptr,
                              0,
                              nullptr,
                              1,
                              &imageBarrier );

        cmdManager.Submit( cmd );
        cmdManager.WaitGraphicsIdle();
    }
}

void RTGL1::RasterPass::DestroyDepthBuffers()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( ( depthImages[ i ] && depthViews[ i ] && depthMemory[ i ] ) ||
                ( !depthImages[ i ] && !depthViews[ i ] && !depthMemory[ i ] ) );

        if( depthImages[ i ] != VK_NULL_HANDLE )
        {
            vkDestroyImage( device, depthImages[ i ], nullptr );
            vkDestroyImageView( device, depthViews[ i ], nullptr );
            vkFreeMemory( device, depthMemory[ i ], nullptr );

            depthImages[ i ] = VK_NULL_HANDLE;
            depthViews[ i ]  = VK_NULL_HANDLE;
            depthMemory[ i ] = VK_NULL_HANDLE;
        }
    }
}
