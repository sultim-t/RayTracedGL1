// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "TextureExporter.h"
#include "Utils.h"

#include "Stb/stb_image_write.h"

#include <span>

namespace
{

constexpr uint64_t DstBytesPerPixel = 4;

bool PrepareTargetFile( const std::filesystem::path& filepath, bool overwriteFiles )
{
    using namespace RTGL1;

    if( std::filesystem::exists( filepath ) )
    {
        if( !overwriteFiles )
        {
            debug::Verbose( "Image was not exported, as file already exists: {}",
                            filepath.string() );
            return false;
        }
        else
        {
            debug::Verbose( "Overwriting existing image file: {}", filepath.string() );
        }
    }
    else
    {
        std::error_code ec;
        std::filesystem::create_directories( filepath.parent_path(), ec );

        if( ec )
        {
            debug::Warning( "{}: std::filesystem::create_directories error: {}",
                            filepath.string(),
                            ec.message() );
            return false;
        }
    }

    return true;
}

}

bool RTGL1::TextureExporter::WriteTGA( std::filesystem::path filepath,
                                       const void*           pixels,
                                       const RgExtent2D&     size )
{
    assert( std::filesystem::exists( filepath.parent_path() ) );

    if( !stbi_write_tga( filepath.replace_extension( ".tga" ).string().c_str(),
                         int( size.width ),
                         int( size.height ),
                         DstBytesPerPixel,
                         pixels ) )
    {
        debug::Warning( "{}: stbi_write_tga fail", filepath.string() );
        return false;
    }

    return true;
}

bool RTGL1::TextureExporter::ExportAsTGA( MemoryAllocator&             allocator,
                                          CommandBufferManager&        cmdManager,
                                          VkImage                      srcImage,
                                          RgExtent2D                   srcImageSize,
                                          VkFormat                     srcImageFormat,
                                          const std::filesystem::path& filepath,
                                          bool                         exportAsSRGB,
                                          bool                         overwriteFiles )
{
    VkDevice device = allocator.GetDevice();

    const VkFormat dstImageFormat =
        exportAsSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    if( !PrepareTargetFile( filepath, overwriteFiles ) )
    {
        return false;
    }

    if( !CheckSupport( allocator.GetPhysicalDevice(), srcImageFormat, dstImageFormat ) )
    {
        return false;
    }

    vkDeviceWaitIdle( device );
    VkCommandBuffer cmd = cmdManager.StartGraphicsCmd();

    constexpr VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkImageSubresourceRange subresRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
    const VkImageSubresource subres = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel   = 0,
        .arrayLayer = 0,
    };

    // Can't vkCmdBlit into a linear tiling directly
    // Can't vkCmdCopy directly from a compressed format (diff block extents with rgba8)
    // 1. Blit from compressed to optimal rgba8
    // 2. Copy from optimal rgba8 to linear rgba8

    VkImage dstImage_Optimal = VK_NULL_HANDLE;
    VkImage dstImage_Linear  = VK_NULL_HANDLE;
    {
        VkImageCreateInfo info = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext       = nullptr,
            .flags       = 0,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = dstImageFormat,
            .extent      = { srcImageSize.width, srcImageSize.height, 1 },
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL, /* optimal */
            .usage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, /* dst + src */
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkResult r = vkCreateImage( device, &info, nullptr, &dstImage_Optimal );
        VK_CHECKERROR( r );
        SET_DEBUG_NAME(
            device, dstImage_Optimal, VK_OBJECT_TYPE_IMAGE, "Export dst image (optimal)" );
    }

    {
        VkImageCreateInfo info = {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = dstImageFormat,
            .extent                = { srcImageSize.width, srcImageSize.height, 1 },
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_LINEAR,          /* linear */
            .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT, /* dst */
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkResult r = vkCreateImage( device, &info, nullptr, &dstImage_Linear );
        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        dstImage_Linear,
                        VK_OBJECT_TYPE_IMAGE,
                        "Export dst image (linear, host-readable)" );
    }

    VkDeviceMemory dstMemory_Optimal = VK_NULL_HANDLE;
    VkDeviceMemory dstMemory_Linear  = VK_NULL_HANDLE;
    {
        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements( device, dstImage_Optimal, &memReqs );

        dstMemory_Optimal = allocator.AllocDedicated( memReqs,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                      MemoryAllocator::AllocType::DEFAULT,
                                                      "Export dst image (optimal)" );

        VkResult r = vkBindImageMemory( device, dstImage_Optimal, dstMemory_Optimal, 0 );
        VK_CHECKERROR( r );
    }
    {
        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements( device, dstImage_Linear, &memReqs );

        dstMemory_Linear = allocator.AllocDedicated( memReqs,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                     MemoryAllocator::AllocType::DEFAULT,
                                                     "Export dst image (linear, host-readable)" );

        VkResult r = vkBindImageMemory( device, dstImage_Linear, dstMemory_Linear, 0 );
        VK_CHECKERROR( r );
    }

    // blit srcImage -> dstImage_Optimal
    {
        VkImageMemoryBarrier2 bs[] = {
            // srcImage to transfer src
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .srcAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout           = srcImageLayout,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = srcImage,
                .subresourceRange    = subresRange,
            },
            // dstImage_Optimal to transfer dst
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .srcAccessMask       = VK_ACCESS_2_NONE,
                .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = dstImage_Optimal,
                .subresourceRange    = subresRange,
            },
        };

        VkDependencyInfoKHR dependencyInfo = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = std::size( bs ),
            .pImageMemoryBarriers    = std::data( bs ),
        };

        svkCmdPipelineBarrier2KHR( cmd, &dependencyInfo );
    }
    {
        VkImageBlit blit = {
            .srcSubresource = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel       = 0,
                                .baseArrayLayer = 0,
                                .layerCount     = 1 },
            .srcOffsets     = { { 0, 0, 0 },
                                { int32_t( srcImageSize.width ), int32_t( srcImageSize.height ), 1 } },
            .dstSubresource = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel       = 0,
                                .baseArrayLayer = 0,
                                .layerCount     = 1 },
            .dstOffsets     = { { 0, 0, 0 },
                                { int32_t( srcImageSize.width ), int32_t( srcImageSize.height ), 1 } },
        };

        vkCmdBlitImage( cmd,
                        srcImage,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        dstImage_Optimal,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit,
                        VK_FILTER_NEAREST );
    }

    // copy dstImage_Optimal -> dstImage_Linear
    {
        VkImageMemoryBarrier2 bs[] = {
            {
                // dstImage_Optimal to transfer src
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = dstImage_Optimal,
                .subresourceRange    = subresRange,
            },
            {
                // dstImage_Linear to transfer dst
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .srcAccessMask       = VK_ACCESS_2_NONE,
                .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = dstImage_Linear,
                .subresourceRange    = subresRange,
            },
        };

        VkDependencyInfoKHR dependencyInfo = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = std::size( bs ),
            .pImageMemoryBarriers    = std::data( bs ),
        };

        svkCmdPipelineBarrier2KHR( cmd, &dependencyInfo );
    }
    {
        VkImageCopy region = {
            .srcSubresource = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel       = 0,
                                .baseArrayLayer = 0,
                                .layerCount     = 1 },
            .srcOffset      = { 0, 0, 0 },
            .dstSubresource = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel       = 0,
                                .baseArrayLayer = 0,
                                .layerCount     = 1 },
            .dstOffset      = { 0, 0, 0 },
            .extent         = { srcImageSize.width, srcImageSize.height, 1 },
        };

        vkCmdCopyImage( cmd,
                        dstImage_Optimal,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        dstImage_Linear,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &region );
    }

    {
        VkImageMemoryBarrier2 bs[] = {
            {
                // srcImage to original layout
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout           = srcImageLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = srcImage,
                .subresourceRange    = subresRange,
            },
            {
                // dstImage_Linear to host read
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_HOST_BIT,
                .dstAccessMask       = VK_ACCESS_2_HOST_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = dstImage_Linear,
                .subresourceRange    = subresRange,
            },
        };

        VkDependencyInfoKHR dependencyInfo = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .imageMemoryBarrierCount = std::size( bs ),
            .pImageMemoryBarriers    = std::data( bs ),
        };

        svkCmdPipelineBarrier2KHR( cmd, &dependencyInfo );
    }

    cmdManager.Submit( cmd );
    cmdManager.WaitGraphicsIdle();

    bool success = false;
    {
        VkSubresourceLayout subresLayout = {};
        vkGetImageSubresourceLayout( device, dstImage_Linear, &subres, &subresLayout );

        if( subresLayout.rowPitch == DstBytesPerPixel * srcImageSize.width )
        {
            assert( subresLayout.size ==
                    DstBytesPerPixel * srcImageSize.width * srcImageSize.height );

            uint8_t* data{ nullptr };
            VkResult r = vkMapMemory( device,
                                      dstMemory_Linear,
                                      0,
                                      VK_WHOLE_SIZE,
                                      0,
                                      reinterpret_cast< void** >( &data ) );
            VK_CHECKERROR( r );

            success = WriteTGA( filepath, &data[ subresLayout.offset ], srcImageSize );

            vkUnmapMemory( device, dstMemory_Linear );
        }
        else
        {
            // manually, if small enough
            if( srcImageSize.width <= 64 && srcImageSize.height <= 64 )
            {
                auto pixels = std::make_unique< uint8_t[] >( DstBytesPerPixel * srcImageSize.width *
                                                             srcImageSize.height );
                {
                    uint8_t* rawData{ nullptr };
                    VkResult r = vkMapMemory( device,
                                              dstMemory_Linear,
                                              0,
                                              VK_WHOLE_SIZE,
                                              0,
                                              reinterpret_cast< void** >( &rawData ) );
                    VK_CHECKERROR( r );

                    uint8_t* beginData = &rawData[ subresLayout.offset ];
                    uint8_t* endData   = &rawData[ subresLayout.offset + subresLayout.size ];

                    uint8_t* dstPtr = pixels.get();

                    for( uint8_t* ptr = beginData; ptr < endData; ptr += subresLayout.rowPitch )
                    {
                        memcpy( dstPtr, ptr, DstBytesPerPixel * srcImageSize.width );
                        dstPtr += DstBytesPerPixel * srcImageSize.width;
                    }

                    vkUnmapMemory( device, dstMemory_Linear );
                }
                success = WriteTGA( filepath, pixels.get(), srcImageSize );
            }
            else
            {
                debug::Warning(
                    "Can't export to image file, as mapped data is not tightly packed: {}. "
                    "VkSubresourceLayout::rowPitch is {}; expected "
                    "( {} bytes per pixel * {} pixels in a row )",
                    filepath.string(),
                    subresLayout.rowPitch,
                    DstBytesPerPixel,
                    srcImageSize.width );
            }
        }
    }
    {
        vkFreeMemory( device, dstMemory_Linear, nullptr );
        vkFreeMemory( device, dstMemory_Optimal, nullptr );

        vkDestroyImage( device, dstImage_Linear, nullptr );
        vkDestroyImage( device, dstImage_Optimal, nullptr );
    }

    return success;
}

bool RTGL1::TextureExporter::CheckSupport( VkPhysicalDevice physDevice,
                                           VkFormat         srcImageFormat,
                                           VkFormat         dstImageFormat )
{
    VkFormatProperties formatProps = {};

    // optimal
    {
        vkGetPhysicalDeviceFormatProperties( physDevice, srcImageFormat, &formatProps );
        if( !( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) )
        {
            debug::Warning( "BLIT_SRC not supported for VkFormat {}", uint32_t( srcImageFormat ) );
            return false;
        }
    }
    {
        vkGetPhysicalDeviceFormatProperties( physDevice, dstImageFormat, &formatProps );
        if( !( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) )
        {
            debug::Warning( "BLIT_DST not supported for VkFormat {}", uint32_t( dstImageFormat ) );
            return false;
        }
        if( !( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT ) )
        {
            debug::Warning( "TRANSFER_SRC not supported for VkFormat {} (linear tiling)",
                            uint32_t( dstImageFormat ) );
            return false;
        }
        if( !( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT ) )
        {
            debug::Warning( "TRANSFER_DST not supported for VkFormat {} (linear tiling)",
                            uint32_t( dstImageFormat ) );
            return false;
        }
    }

    return true;
}
