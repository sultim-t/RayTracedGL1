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

#include "TextureUploader.h"
#include "Utils.h"

TextureUploader::TextureUploader(VkDevice _device, std::shared_ptr<MemoryAllocator> _memAllocator)
    : device(_device), memAllocator(std::move(_memAllocator))
{}

TextureUploader::~TextureUploader()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (VkBuffer staging : stagingToFree[i])
        {
            memAllocator->DestroyStagingSrcTextureBuffer(staging);
        }
    }
}

void TextureUploader::ClearStaging(uint32_t frameIndex)
{
    // clear unused staging
    for (VkBuffer staging : stagingToFree[frameIndex])
    {
        memAllocator->DestroyStagingSrcTextureBuffer(staging);
    }

    stagingToFree[frameIndex].clear();
}

uint32_t TextureUploader::GetMipmapCount(const RgExtent2D &size)
{
    auto widthCount = static_cast<uint32_t>(log2(size.width));
    auto heightCount = static_cast<uint32_t>(log2(size.height));

    return std::min(widthCount, heightCount) + 1;
}

TextureUploader::UploadResult TextureUploader::UploadStaticImage(VkCommandBuffer cmd, uint32_t frameIndex, const void *data, const RgExtent2D &size, const char *debugName)
{
    UploadResult result = {};
    result.wasUploaded = false;

    VkResult r;

    // 1. Allocate and fill buffer

    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const VkDeviceSize bytesPerPixel = 4;

    VkDeviceSize dataSize = bytesPerPixel * size.width * size.height;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkDeviceMemory stagingMemory, imageMemory;
    void *mappedData;

    VkBuffer stagingBuffer = memAllocator->CreateStagingSrcTextureBuffer(&stagingInfo, &stagingMemory, &mappedData);
    if (stagingBuffer == VK_NULL_HANDLE)
    {
        return result;
    }

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, stagingBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, debugName);
    }

    // copy image data to buffer
    memcpy(mappedData, data, dataSize);

    uint32_t mipmapCount = GetMipmapCount(size);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent = { size.width, size.height, 1 };
    imageInfo.mipLevels = mipmapCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImage finalImage = memAllocator->CreateDstTextureImage(&imageInfo, &imageMemory);
    if (finalImage == VK_NULL_HANDLE)
    {
        memAllocator->DestroyStagingSrcTextureBuffer(stagingBuffer);

        return result;
    }

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, finalImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, debugName);
    }

    // 2. Copy buffer data to the first mipmap

    VkImageSubresourceRange firstMipmap = {};
    firstMipmap.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    firstMipmap.baseMipLevel = 0;
    firstMipmap.levelCount = 1;
    firstMipmap.baseArrayLayer = 0;
    firstMipmap.layerCount = 1;

    // set layout for copying
    Utils::BarrierImage(
        cmd, finalImage,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        firstMipmap);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    // tigthly packed
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageExtent = { size.width, size.height, 1 };
    copyRegion.imageOffset = { 0,0,0 };
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(
        cmd, stagingBuffer, finalImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // 3. Create mipmaps

    // first mipmap to TRANSFER_SRC to create mipmaps using blit
    Utils::BarrierImage(
        cmd, finalImage,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        firstMipmap);

    uint32_t mipWidth = size.width;
    uint32_t mipHeight = size.height;

    for (uint32_t mipLevel = 1; mipLevel < mipmapCount; mipLevel++)
    {
        uint32_t prevMipWidth = mipWidth;
        uint32_t prevMipHeight = mipHeight;

        mipWidth >>= 1;
        mipHeight >>= 1;

        assert(mipWidth > 0 && mipHeight > 0);
        assert(mipLevel != mipmapCount - 1 || (mipWidth == 1 || mipHeight == 1));

        VkImageSubresourceRange curMipmap = {};
        curMipmap.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curMipmap.baseMipLevel = mipLevel;
        curMipmap.levelCount = 1;
        curMipmap.baseArrayLayer = 0;
        curMipmap.layerCount = 1;

        // current mip to TRANSFER_DST
        Utils::BarrierImage(
            cmd, finalImage,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);

        // blit from previous mip level
        VkImageBlit curBlit = {};

        curBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.srcSubresource.mipLevel = mipLevel - 1;
        curBlit.srcSubresource.baseArrayLayer = 0;
        curBlit.srcSubresource.layerCount = 1;
        curBlit.srcOffsets[0] = { 0,0,0 };
        curBlit.srcOffsets[1] = { (int32_t)prevMipWidth, (int32_t)prevMipHeight, 1 };

        curBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.dstSubresource.mipLevel = mipLevel;
        curBlit.dstSubresource.baseArrayLayer = 0;
        curBlit.dstSubresource.layerCount = 1;
        curBlit.dstOffsets[0] = { 0,0,0 };
        curBlit.dstOffsets[1] = { (int32_t)mipWidth, (int32_t)mipHeight, 1 };

        vkCmdBlitImage(
            cmd,
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &curBlit, VK_FILTER_LINEAR);

        // current mip to TRANSFER_SRC for the next one
        Utils::BarrierImage(
            cmd, finalImage,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);
    }

    // 4. Prepare all mipmaps for reading in ray tracing and fragment shaders

    VkImageSubresourceRange allMipmaps = {};
    allMipmaps.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    allMipmaps.baseMipLevel = 0;
    allMipmaps.levelCount = mipmapCount;
    allMipmaps.baseArrayLayer = 0;
    allMipmaps.layerCount = 1;

    Utils::BarrierImage(
        cmd, finalImage,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        allMipmaps);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = finalImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.components = {};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipmapCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView finalImageView;
    r = vkCreateImageView(device, &viewInfo, nullptr, &finalImageView);
    VK_CHECKERROR(r);

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, finalImageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, debugName);
    }

    // push staging buffer to be deleted when it won't be in use
    stagingToFree[frameIndex].push_back(stagingBuffer);

    result.wasUploaded = true;
    result.image = finalImage;
    result.view = finalImageView;
    return result;
}

void TextureUploader::DestroyImage(VkImage image, VkImageView view)
{
    memAllocator->DestroyTextureImage(image);
    vkDestroyImageView(device, view, nullptr);
}
