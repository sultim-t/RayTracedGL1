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

#include "Framebuffers.h"

using namespace RTGL1;

#include "Swapchain.h"
#include "Utils.h"
#include "CmdLabel.h"

#include <vector>

static_assert(MAX_FRAMES_IN_FLIGHT == FRAMEBUFFERS_HISTORY_LENGTH, "Framebuffers class logic must be changed if history length is not equal to max frames in flight");

FramebufferImageIndex Framebuffers::FrameIndexToFBIndex(FramebufferImageIndex framebufferImageIndex, uint32_t frameIndex)
{
    assert(frameIndex < FRAMEBUFFERS_HISTORY_LENGTH);
    assert(framebufferImageIndex >= 0 && framebufferImageIndex < ShFramebuffers_Count);

    // if framubuffer with given index can be swapped,
    // use one that is currently in use
    if (ShFramebuffers_Bindings[framebufferImageIndex] != ShFramebuffers_BindingsSwapped[framebufferImageIndex])
    {
        return (FramebufferImageIndex)(framebufferImageIndex + frameIndex);
    }

    return framebufferImageIndex;
}

Framebuffers::Framebuffers(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> _allocator, 
    std::shared_ptr<CommandBufferManager> _cmdManager)
: 
    device(_device),
    bilinearSampler(VK_NULL_HANDLE),
    nearestSampler(VK_NULL_HANDLE),
    allocator(std::move(_allocator)),
    cmdManager(std::move(_cmdManager)),
    currentResolution{},
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{}
{
    images.resize(ShFramebuffers_Count);
    imageMemories.resize(ShFramebuffers_Count);
    imageViews.resize(ShFramebuffers_Count);

    CreateDescriptors();
    CreateSamplers();
}

Framebuffers::~Framebuffers()
{
    DestroyImages();

    vkDestroySampler(device, nearestSampler, nullptr);
    vkDestroySampler(device, bilinearSampler, nullptr);
    
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
}

void Framebuffers::CreateDescriptors()
{
    VkResult r;

    const uint32_t allBindingsCount = ShFramebuffers_Count * 2;
    const uint32_t samplerBindingOffset = ShFramebuffers_Count;

    std::vector<VkDescriptorSetLayoutBinding> bindings(allBindingsCount);
    uint32_t bndCount = 0;

    // gimage2D
    for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
    {
        VkDescriptorSetLayoutBinding &bnd = bindings[bndCount];

        // after swapping bindings, cur will become prev, and prev - cur
        bnd.binding = ShFramebuffers_Bindings[i];
        bnd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bnd.descriptorCount = 1;
        bnd.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        bndCount++;
    }

    // gsampler2D
    for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
    {
        VkDescriptorSetLayoutBinding &bnd = bindings[bndCount];

        if (ShFramebuffers_Sampler_Bindings[i] == FB_SAMPLER_INVALID_BINDING)
        {
            continue;
        }

        // after swapping bindings, cur will become prev, and prev - cur
        bnd.binding = ShFramebuffers_Sampler_Bindings[i];
        bnd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bnd.descriptorCount = 1;
        bnd.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        bndCount++;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bndCount;
    layoutInfo.pBindings = bindings.data();

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Framebuffers Desc set layout");

    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = allBindingsCount * FRAMEBUFFERS_HISTORY_LENGTH;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = allBindingsCount * FRAMEBUFFERS_HISTORY_LENGTH;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = FRAMEBUFFERS_HISTORY_LENGTH;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Framebuffers Desc pool");

    for (uint32_t i = 0; i < FRAMEBUFFERS_HISTORY_LENGTH; i++)
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descSetLayout;

        r = vkAllocateDescriptorSets(device, &allocInfo, &descSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSets[i], VK_OBJECT_TYPE_DESCRIPTOR_SET, "Framebuffers Desc set");
    }
}

void RTGL1::Framebuffers::CreateSamplers()
{
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.addressModeU = info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.anisotropyEnable = VK_FALSE;
    info.maxAnisotropy = 0;
    info.compareEnable = VK_FALSE;
    info.minLod = 0.0f;
    info.maxLod = VK_LOD_CLAMP_NONE;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;

    {
        info.minFilter = info.magFilter = VK_FILTER_NEAREST;

        VkResult r = vkCreateSampler(device, &info, nullptr, &nearestSampler);
        VK_CHECKERROR(r);
    }

    {
        info.minFilter = info.magFilter = VK_FILTER_LINEAR;

        VkResult r = vkCreateSampler(device, &info, nullptr, &bilinearSampler);
        VK_CHECKERROR(r);
    }
}

bool RTGL1::Framebuffers::PrepareForSize(ResolutionState resolutionState)
{
    if (currentResolution == resolutionState)
    {
        return false;
    }

    vkDeviceWaitIdle(device);

    DestroyImages();
    CreateImages(resolutionState);

    assert(currentResolution == resolutionState);
    return true;
}

void RTGL1::Framebuffers::BarrierOne(VkCommandBuffer cmd, uint32_t frameIndex, FramebufferImageIndex framebufImageIndex, BarrierType barrierTypeFrom)
{
    FramebufferImageIndex fs[] = { framebufImageIndex };
    BarrierMultiple(cmd, frameIndex, fs, barrierTypeFrom);
}

void Framebuffers::PresentToSwapchain(
    VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Swapchain> &swapchain,
    FramebufferImageIndex framebufImageIndex, VkFilter filter)
{
    CmdLabel label(cmd, "Present to swapchain");

    BarrierOne(cmd, frameIndex, framebufImageIndex);

    VkExtent2D srcExtent = GetFramebufSize(ShFramebuffers_Flags[framebufImageIndex], currentResolution);

    swapchain->BlitForPresent(
        cmd, GetImage(framebufImageIndex, frameIndex),
        srcExtent.width, srcExtent.height, filter, VK_IMAGE_LAYOUT_GENERAL);
}

namespace
{
    VkOffset3D ToSigned( const VkExtent2D& e )
    {
        return VkOffset3D{
            .x = static_cast< int32_t >( e.width ),
            .y = static_cast< int32_t >( e.height ),
            .z = 1,
        };
    }

    const RgExtent2D* MakeSafePixelized( const RgExtent2D*             pPixelizedRenderSize,
                                         const RTGL1::ResolutionState& resolution )
    {
        if( pPixelizedRenderSize == nullptr)
        {
            return nullptr;
        }

        if( std::abs( static_cast< int32_t >( resolution.renderWidth ) -
                      static_cast< int32_t >( pPixelizedRenderSize->width ) ) < 8 )
        {
            return nullptr;
        }

        if( std::abs( static_cast< int32_t >( resolution.renderHeight ) -
                      static_cast< int32_t >( pPixelizedRenderSize->height ) ) < 8 )
        {
            return nullptr;
        }

        return pPixelizedRenderSize;
    }

    VkOffset3D NormalizePixelized( const RgExtent2D &pPixelizedRenderSize, const RTGL1::ResolutionState &resolution )
    {
        return VkOffset3D{
            .x = std::clamp( static_cast< int32_t >( pPixelizedRenderSize.width ),
                             8,
                             static_cast< int32_t >( resolution.renderWidth ) ),
            .y = std::clamp( static_cast< int32_t >( pPixelizedRenderSize.height ),
                             8,
                             static_cast< int32_t >( resolution.renderHeight ) ),
            .z = 1,
        };
    }
}

RTGL1::FramebufferImageIndex RTGL1::Framebuffers::BlitForEffects(
    VkCommandBuffer       cmd,
    uint32_t              frameIndex,
    FramebufferImageIndex framebufImageIndex,
    VkFilter              filter,
    const RgExtent2D*     pPixelizedRenderSize )
{
    pPixelizedRenderSize = MakeSafePixelized( pPixelizedRenderSize, currentResolution );

    const VkImageSubresourceRange subresRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    const VkImageSubresourceLayers subresLayers = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel       = 0,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    const FramebufferImageIndex src = FrameIndexToFBIndex( framebufImageIndex, frameIndex );
    assert( src == FB_IMAGE_INDEX_FINAL || src == FB_IMAGE_INDEX_UPSCALED_PING || src == FB_IMAGE_INDEX_UPSCALED_PONG );

    const FramebufferImageIndex dst =
        src == FB_IMAGE_INDEX_FINAL         ? FrameIndexToFBIndex( FB_IMAGE_INDEX_UPSCALED_PING, frameIndex ) :
        src == FB_IMAGE_INDEX_UPSCALED_PING ? FrameIndexToFBIndex( FB_IMAGE_INDEX_UPSCALED_PONG, frameIndex ) : 
        src == FB_IMAGE_INDEX_UPSCALED_PONG ? FrameIndexToFBIndex( FB_IMAGE_INDEX_UPSCALED_PING, frameIndex ) : FB_IMAGE_INDEX_UPSCALED_PING;

    const bool fromFinal = src == FB_IMAGE_INDEX_FINAL;

    const VkImage srcImage = images[ src ];
    const VkImage dstImage = images[ dst ];

    const VkOffset3D srcExtent      = ToSigned( GetFramebufSize( ShFramebuffers_Flags[ src ], currentResolution ) );
    const VkOffset3D upscaledExtent = ToSigned( GetFramebufSize( ShFramebuffers_Flags[ dst ], currentResolution ) );

    const VkOffset3D dstExtent =
        pPixelizedRenderSize ? NormalizePixelized( *pPixelizedRenderSize, currentResolution )
                             : upscaledExtent;

    if( pPixelizedRenderSize )
    {
        filter = VK_FILTER_LINEAR;
    }

    // if source has almost the same size as the surface, then use nearest blit
    if( std::abs( srcExtent.x - dstExtent.x ) < 8 && std::abs( srcExtent.y - dstExtent.y ) < 8 )
    {
        filter = VK_FILTER_NEAREST;
    }

    // sync for blit, new layouts
    {
        VkImageMemoryBarrier2 bs[] = {
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = srcImage,
                .subresourceRange    = subresRange,
            },
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = dstImage,
                .subresourceRange    = subresRange,
            },
        };

        VkDependencyInfo dep = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = std::size( bs ),
            .pImageMemoryBarriers    = bs,
        };

        svkCmdPipelineBarrier2KHR( cmd, &dep );
    }

    {
        const VkImageBlit region = {
            .srcSubresource = subresLayers,
            .srcOffsets     = { { 0, 0, 0 }, srcExtent },
            .dstSubresource = subresLayers,
            .dstOffsets     = { { 0, 0, 0 }, dstExtent },
        };

        vkCmdBlitImage( cmd,
                        srcImage,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        dstImage,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &region,
                        filter );
    }

    VkImage               layoutRestoreSrc;
    VkImage               layoutRestoreDst;
    FramebufferImageIndex finalDst;

    // if need another blit to rescale from pixelated
    if( pPixelizedRenderSize )
    {
        FramebufferImageIndex newSrc = dst;
        FramebufferImageIndex newDst;

        switch( newSrc )
        {
            case FB_IMAGE_INDEX_UPSCALED_PING: newDst = FrameIndexToFBIndex( FB_IMAGE_INDEX_UPSCALED_PONG, frameIndex ); break;
            case FB_IMAGE_INDEX_UPSCALED_PONG: newDst = FrameIndexToFBIndex( FB_IMAGE_INDEX_UPSCALED_PING, frameIndex ); break;
        
            default: assert( 0 ); newDst = FB_IMAGE_INDEX_UPSCALED_PING; break;
        }

        VkImage newSrcImage = images[ newSrc ];
        VkImage newDstImage = images[ newDst ];

        const VkOffset3D &newSrcExtent = dstExtent;
        const VkOffset3D &newDstExtent = upscaledExtent;

        {
            VkImageMemoryBarrier2 bs[] = {
                {
                    .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                    .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = newSrcImage,
                    .subresourceRange    = subresRange,
                },
                {
                    .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                    .srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                    // if from final, then we haven't touch other ping/pong image,
                    // so its layout is general
                    .oldLayout           = fromFinal ? VK_IMAGE_LAYOUT_GENERAL
                                                     : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = newDstImage,
                    .subresourceRange    = subresRange,
                },
            };

            VkDependencyInfo dep = {
                .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = std::size( bs ),
                .pImageMemoryBarriers    = bs,
            };

            svkCmdPipelineBarrier2KHR( cmd, &dep );
        }

        {
            const VkImageBlit region = {
                .srcSubresource = subresLayers,
                .srcOffsets     = { { 0, 0, 0 }, newSrcExtent },
                .dstSubresource = subresLayers,
                .dstOffsets     = { { 0, 0, 0 }, newDstExtent },
            };

            vkCmdBlitImage( cmd,
                            newSrcImage,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            newDstImage,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1,
                            &region,
                            VK_FILTER_NEAREST );
        }

        layoutRestoreSrc = newSrcImage;
        layoutRestoreDst = newDstImage;
        finalDst         = newDst;
    }
    else
    { 
        layoutRestoreSrc = srcImage;
        layoutRestoreDst = dstImage;
        finalDst         = dst;
    }

    // wait for blit and restore layouts
    {
        VkImageMemoryBarrier2 bs[] = {
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = layoutRestoreSrc,
                .subresourceRange    = subresRange,
            },
            {
                .sType              = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask       = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask      = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstStageMask       = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .dstAccessMask      = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |  
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = layoutRestoreDst,
                .subresourceRange    = subresRange,
            },
            // optional: consider transitioning FB_IMAGE_INDEX_FINAL,
            // if from final and ping/pong was intermediately pixelized
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = srcImage,
                .subresourceRange    = subresRange,
            },
        };

        VkDependencyInfo dep = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = fromFinal && pPixelizedRenderSize ? 3u : 2u,
            .pImageMemoryBarriers    = bs,
        };

        svkCmdPipelineBarrier2KHR( cmd, &dep );
    }

    return finalDst;
}

VkDescriptorSet Framebuffers::GetDescSet(uint32_t frameIndex) const
{
    return descSets[frameIndex];
}

VkDescriptorSetLayout Framebuffers::GetDescSetLayout() const
{
    return descSetLayout;
}

VkImage Framebuffers::GetImage(FramebufferImageIndex fbImageIndex, uint32_t frameIndex) const
{
    fbImageIndex = FrameIndexToFBIndex(fbImageIndex, frameIndex);
    return images[fbImageIndex];
}

VkImageView Framebuffers::GetImageView(FramebufferImageIndex fbImageIndex, uint32_t frameIndex) const
{
    fbImageIndex = FrameIndexToFBIndex(fbImageIndex, frameIndex);
    return imageViews[fbImageIndex];
}

std::tuple<VkImage, VkImageView, VkFormat> RTGL1::Framebuffers::GetImageHandles(FramebufferImageIndex fbImageIndex, uint32_t frameIndex) const
{
    fbImageIndex = FrameIndexToFBIndex(fbImageIndex, frameIndex);

    return std::make_tuple(images[fbImageIndex], imageViews[fbImageIndex], ShFramebuffers_Formats[fbImageIndex]);
}

std::tuple<VkImage, VkImageView, VkFormat, VkExtent2D> Framebuffers::GetImageHandles(
    FramebufferImageIndex fbImageIndex, uint32_t frameIndex, const ResolutionState&resolutionState) const
{
    auto [image, view, format] = GetImageHandles(fbImageIndex, frameIndex);
    
    return std::make_tuple(image, view, format, GetFramebufSize(ShFramebuffers_Flags[fbImageIndex], resolutionState));
}

VkExtent2D RTGL1::Framebuffers::GetFramebufSize(FramebufferImageFlags flags, const ResolutionState &resolutionState)
{
    if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_UPSCALED_SIZE)
    {
        return { resolutionState.upscaledWidth, resolutionState.upscaledHeight };
    }

    if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE)
    {
        return { 1,1 };
    }

    int downscale = 1;

    if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_2)
    {
        downscale = 2;
    }
    else if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_3)
    {
        downscale = 3;
    }
    else if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_4)
    {
        downscale = 4;
    }
    else if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_8)
    {
        downscale = 8;
    }
    else if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_16)
    {
        downscale = 16;
    }
    else if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_32)
    {
        downscale = 32;
    }
    else
    {
        return { resolutionState.renderWidth, resolutionState.renderHeight };
    }

    VkExtent2D extent;

    extent.width  = (resolutionState.renderWidth  + 1) / downscale;
    extent.height = (resolutionState.renderHeight + 1) / downscale;

    extent.width  = std::max(1u, extent.width);
    extent.height = std::max(1u, extent.height);

    return extent;
}

void Framebuffers::CreateImages(ResolutionState resolutionState)
{
    VkResult r;

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
    {
        VkFormat format = ShFramebuffers_Formats[i];
        FramebufferImageFlags flags = ShFramebuffers_Flags[i];

        VkExtent2D extent = GetFramebufSize(flags, resolutionState);

        // create image
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = { extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = 
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT; 
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_IS_ATTACHMENT)
        {
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if (flags & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_USAGE_TRANSFER)
        {
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        r = vkCreateImage(device, &imageInfo, nullptr, &images[i]);
        VK_CHECKERROR(r);

        // allocate dedicated memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, images[i], &memReqs);

        imageMemories[i] = allocator->AllocDedicated(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryAllocator::AllocType::DEFAULT, ShFramebuffers_DebugNames[i]);

        r = vkBindImageMemory(device, images[i], imageMemories[i], 0);
        VK_CHECKERROR(r);

        // create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.image = images[i];
        r = vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, images[i], VK_OBJECT_TYPE_IMAGE, ShFramebuffers_DebugNames[i]);
        SET_DEBUG_NAME(device, imageViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, ShFramebuffers_DebugNames[i]);

        // to general layout
        Utils::BarrierImage(
            cmd, images[i],
            0, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }

    // image creation happens rarely
    cmdManager->Submit(cmd);
    cmdManager->WaitGraphicsIdle();

    currentResolution = resolutionState;


    UpdateDescriptors();

    NotifySubscribersAboutResize(resolutionState);
}

void Framebuffers::UpdateDescriptors()
{
    const uint32_t allBindingsCount = ShFramebuffers_Count * 2;
    const uint32_t samplerBindingOffset = ShFramebuffers_Count;

    std::vector<VkDescriptorImageInfo> imageInfos(allBindingsCount);

    // gimage2D
    for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
    {
        imageInfos[i].sampler = VK_NULL_HANDLE;
        imageInfos[i].imageView = imageViews[i];
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    // gsampler2D
    for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
    {    
        // texelFetch should be used to get a specific texel,
        // and texture/textureLod for sampling with bilinear interpolation

        bool useBilinear = ShFramebuffers_Flags[i] & FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_BILINEAR_SAMPLER;

        imageInfos[samplerBindingOffset + i].sampler = useBilinear ? bilinearSampler : nearestSampler;
        imageInfos[samplerBindingOffset + i].imageView = imageViews[i];
        imageInfos[samplerBindingOffset + i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    std::vector<VkWriteDescriptorSet> writes(allBindingsCount * FRAMEBUFFERS_HISTORY_LENGTH);
    uint32_t wrtCount = 0;

    for (uint32_t k = 0; k < FRAMEBUFFERS_HISTORY_LENGTH; k++)
    {
        // gimage2D
        for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
        {
            auto &wrt = writes[wrtCount];

            wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wrt.dstSet = descSets[k];
            wrt.dstBinding = k == 0 ?
                ShFramebuffers_Bindings[i] :
                ShFramebuffers_BindingsSwapped[i];
            wrt.dstArrayElement = 0;
            wrt.descriptorCount = 1;
            wrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            wrt.pImageInfo = &imageInfos[i];

            wrtCount++;
        }

        // gsampler2D
        for (uint32_t i = 0; i < ShFramebuffers_Count; i++)
        {
            auto &wrt = writes[wrtCount];

            uint32_t dstBinding = k == 0 ?
                ShFramebuffers_Sampler_Bindings[i] :
                ShFramebuffers_Sampler_BindingsSwapped[i];

            if (dstBinding == FB_SAMPLER_INVALID_BINDING)
            {
                continue;
            }

            wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wrt.dstSet = descSets[k];
            wrt.dstBinding = dstBinding;
            wrt.dstArrayElement = 0;
            wrt.descriptorCount = 1;
            wrt.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wrt.pImageInfo = &imageInfos[samplerBindingOffset + i];

            wrtCount++;
        }
    }

    vkUpdateDescriptorSets(device, wrtCount, writes.data(), 0, nullptr);
}

void Framebuffers::DestroyImages()
{
    for (auto &i : images)
    {
        if (i != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, i, nullptr);
            i = VK_NULL_HANDLE;
        }
    }

    for (auto &m : imageMemories)
    {
        if (m != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m, nullptr);
            m = VK_NULL_HANDLE;
        }
    }

    for (auto &v : imageViews)
    {
        if (v != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, v, nullptr);
            v = VK_NULL_HANDLE;
        }
    }
}

void Framebuffers::NotifySubscribersAboutResize(const ResolutionState &resolutionState)
{
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            s->OnFramebuffersSizeChange(resolutionState);
        }
    }
}

void Framebuffers::Subscribe(std::shared_ptr<IFramebuffersDependency> subscriber)
{
    subscribers.emplace_back(subscriber);
}

void Framebuffers::Unsubscribe(const IFramebuffersDependency *subscriber)
{
    subscribers.remove_if([subscriber] (const std::weak_ptr<IFramebuffersDependency> &ws)
    {
        if (const auto s = ws.lock())
        {
            return s.get() == subscriber;
        }

        return true;
    });
}
