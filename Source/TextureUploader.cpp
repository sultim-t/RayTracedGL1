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

using namespace RTGL1;

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

    for (auto &p : dynamicImageInfos)
    {
        memAllocator->DestroyStagingSrcTextureBuffer(p.second.stagingBuffer);
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

uint32_t TextureUploader::GetMipmapCount(const RgExtent2D &size, bool generateMipmaps)
{
    if (!generateMipmaps)
    {
        return 1;
    }

    auto widthCount = static_cast<uint32_t>(log2(size.width));
    auto heightCount = static_cast<uint32_t>(log2(size.height));

    return std::min(widthCount, heightCount) + 1;
}

void TextureUploader::PrepareMipmaps(VkCommandBuffer cmd, VkImage image, uint32_t baseWidth, uint32_t baseHeight, uint32_t mipmapCount, uint32_t layerCount)
{
    if (mipmapCount <= 1)
    {
        return;
    }

    uint32_t mipWidth = baseWidth;
    uint32_t mipHeight = baseHeight;

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
        curMipmap.layerCount = layerCount;

        // current mip to TRANSFER_DST
        Utils::BarrierImage(
            cmd, image,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);

        // blit from previous mip level
        VkImageBlit curBlit = {};

        curBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.srcSubresource.mipLevel = mipLevel - 1;
        curBlit.srcSubresource.baseArrayLayer = 0;
        curBlit.srcSubresource.layerCount = layerCount;
        curBlit.srcOffsets[0] = { 0,0,0 };
        curBlit.srcOffsets[1] = { (int32_t)prevMipWidth, (int32_t)prevMipHeight, 1 };

        curBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.dstSubresource.mipLevel = mipLevel;
        curBlit.dstSubresource.baseArrayLayer = 0;
        curBlit.dstSubresource.layerCount = layerCount;
        curBlit.dstOffsets[0] = { 0,0,0 };
        curBlit.dstOffsets[1] = { (int32_t)mipWidth, (int32_t)mipHeight, 1 };

        vkCmdBlitImage(
            cmd,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &curBlit, VK_FILTER_LINEAR);

        // current mip to TRANSFER_SRC for the next one
        Utils::BarrierImage(
            cmd, image,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);
    }
}

void TextureUploader::CopyStagingToImage(VkCommandBuffer cmd, VkBuffer staging, VkImage image, const RgExtent2D &size, uint32_t layerCount)
{
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
    copyRegion.imageSubresource.layerCount = layerCount;

    vkCmdCopyBufferToImage(
        cmd, staging, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

bool TextureUploader::CreateImage(const UploadInfo &info, VkImage *result)
{
    const RgExtent2D &size = info.size;
 
    // 1. Create image and allocate its memory

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.flags = info.isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    imageInfo.format = info.format;
    imageInfo.extent = { size.width, size.height, 1 };
    imageInfo.mipLevels = GetMipmapCount(size, info.generateMipmaps);
    imageInfo.arrayLayers = info.isCubemap ? 6 : 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImage image = memAllocator->CreateDstTextureImage(&imageInfo);
    if (image == VK_NULL_HANDLE)
    {
        return false;
    }

    SET_DEBUG_NAME(device, image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, info.debugName);

    *result = image;
    return true;
}

void TextureUploader::PrepareImage(VkImage image, VkBuffer staging, const UploadInfo &info, ImagePrepareType prepareType)
{
    VkCommandBuffer     cmd             = info.cmd;
    const RgExtent2D    &size           = info.size;
    bool                generateMipmaps = info.generateMipmaps;
    uint32_t            layerCount      = info.isCubemap ? 6 : 1;

    uint32_t mipmapCount = GetMipmapCount(size, generateMipmaps);

    // 2. Copy buffer data to the first mipmap

    VkImageSubresourceRange firstMipmap = {};
    firstMipmap.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    firstMipmap.baseMipLevel = 0;
    firstMipmap.levelCount = 1;
    firstMipmap.baseArrayLayer = 0;
    firstMipmap.layerCount = layerCount;

    VkAccessFlags curAccessMask;
    VkImageLayout curLayout;
    VkPipelineStageFlags curStageMask;

    // if image was already prepared
    if (prepareType == ImagePrepareType::UPDATE)
    {
        curAccessMask = VK_ACCESS_SHADER_READ_BIT;
        curLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        curStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        curAccessMask = 0;
        curLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        curStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    // if need to copy from staging
    if (prepareType != ImagePrepareType::INIT_WITHOUT_COPYING)
    {
        // set layout for copying
        Utils::BarrierImage(
            cmd, image,
            curAccessMask, VK_ACCESS_TRANSFER_WRITE_BIT,
            curLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            curStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT,
            firstMipmap);

        // update params
        curAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        curLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        curStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        CopyStagingToImage(cmd, staging, image, size, layerCount);
    }

    if (mipmapCount > 1)
    {
        // 3A. 1. Create mipmaps

        // first mipmap to TRANSFER_SRC to create mipmaps using blit
        Utils::BarrierImage(
            cmd, image,
            curAccessMask, VK_ACCESS_TRANSFER_READ_BIT,
            curLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            curStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT,
            firstMipmap);

        PrepareMipmaps(cmd, image, size.width, size.height, mipmapCount, layerCount);

        // 3A. 2. Prepare all mipmaps for reading in ray tracing and fragment shaders

        VkImageSubresourceRange allMipmaps = {};
        allMipmaps.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        allMipmaps.baseMipLevel = 0;
        allMipmaps.levelCount = mipmapCount;
        allMipmaps.baseArrayLayer = 0;
        allMipmaps.layerCount = layerCount;

        Utils::BarrierImage(
            cmd, image,
            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            allMipmaps);
    }
    else
    {
        // 3B. Prepare only the first mipmap for reading in ray tracing and fragment shaders

        Utils::BarrierImage(
            cmd, image,
            curAccessMask, VK_ACCESS_SHADER_READ_BIT,
            curLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            curStageMask, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            firstMipmap);
    }
}

VkImageView TextureUploader::CreateImageView(VkImage image, VkFormat format, bool isCubemap, uint32_t mipmapCount)
{
    VkImageView view;

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = isCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipmapCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = isCubemap ? 6 : 1;

    VkResult r = vkCreateImageView(device, &viewInfo, nullptr, &view);
    VK_CHECKERROR(r);

    return view;
}

TextureUploader::UploadResult TextureUploader::UploadImage(const UploadInfo &info)
{
    const void          *data   = info.data;
    const RgExtent2D    &size   = info.size;

    // static textures must not have null data
    assert(info.isDynamic || data != nullptr);

    UploadResult result = {};
    result.wasUploaded = false;

    VkResult r;
    void *mappedData;
    VkImage image;

    // 1. Allocate and fill buffer
    uint32_t faceNumber = info.isCubemap ? 6 : 1;
    VkDeviceSize faceSize = (VkDeviceSize)info.bytesPerPixel * size.width * size.height;
    VkDeviceSize dataSize = faceSize * faceNumber;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkBuffer stagingBuffer = memAllocator->CreateStagingSrcTextureBuffer(&stagingInfo, &mappedData);
    if (stagingBuffer == VK_NULL_HANDLE)
    {
        return result;
    }

    SET_DEBUG_NAME(device, stagingBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, info.debugName);

    bool wasCreated = CreateImage(info, &image);
    if (!wasCreated)
    {
        // clean created resources
        memAllocator->DestroyStagingSrcTextureBuffer(stagingBuffer);
        return result;
    }

    // if it's a dynamic texture and the data is not provided yet
    if (info.isDynamic && data == nullptr)
    {
        // create image without copying
        PrepareImage(image, VK_NULL_HANDLE, info, ImagePrepareType::INIT_WITHOUT_COPYING);
    }
    else
    {
        // copy image data to buffer
        if (info.isCubemap)
        {
            assert(faceNumber == 6);

            for (uint32_t i = 0; i < 6; i++)
            {
                memcpy((uint8_t*)mappedData + i * faceSize, info.cubemap.faces[i], faceSize);
            }
        }
        else
        {
            assert(faceSize == dataSize);

            memcpy(mappedData, data, dataSize);
        }
        // and copy it to image
        PrepareImage(image, stagingBuffer, info, ImagePrepareType::INIT);
    }

    // create image view
    VkImageView imageView = CreateImageView(image, info.format, info.isCubemap, GetMipmapCount(size, info.generateMipmaps));

    SET_DEBUG_NAME(device, imageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, info.debugName);

    // save info about created image
    if (info.isDynamic)
    {
        // for dynamic images:
        // save pointer for updating image data
        DynamicImageInfo updateInfo = {};
        updateInfo.stagingBuffer = stagingBuffer;
        updateInfo.mappedData = mappedData;
        updateInfo.dataSize = (uint32_t)dataSize;
        updateInfo.imageSize = size;
        updateInfo.generateMipmaps = info.generateMipmaps;

        dynamicImageInfos[image] = updateInfo;
    }
    else
    {
        // for static images that won't be updated:
        // push staging buffer to be deleted when it won't be in use
        stagingToFree[info.frameIndex].push_back(stagingBuffer);
    }

    // return results
    result.wasUploaded = true;
    result.image = image;
    result.view = imageView;
    return result;
}

void TextureUploader::UpdateDynamicImage(VkCommandBuffer cmd, VkImage dynamicImage, const void *data)
{
    assert(dynamicImage != VK_NULL_HANDLE);

    auto it = dynamicImageInfos.find(dynamicImage);

    if (it != dynamicImageInfos.end())
    {
        auto &updateInfo = it->second;

        assert(updateInfo.mappedData != nullptr);
        memcpy(updateInfo.mappedData, data, updateInfo.dataSize);

        UploadInfo info = {};
        info.cmd = cmd;
        info.size = updateInfo.imageSize;
        info.generateMipmaps = updateInfo.generateMipmaps;

        // copy from staging
        PrepareImage(dynamicImage, updateInfo.stagingBuffer, info, ImagePrepareType::UPDATE);
    }
}

void TextureUploader::DestroyImage(VkImage image, VkImageView view)
{
    auto it = dynamicImageInfos.find(image);

    // if it's a dynamic texture
    if (it != dynamicImageInfos.end())
    {
        // destroy its staging buffer, as it exists for
        // whole dynamic image lifetime
        memAllocator->DestroyStagingSrcTextureBuffer(it->second.stagingBuffer);

        dynamicImageInfos.erase(it);
    }

    memAllocator->DestroyTextureImage(image);
    vkDestroyImageView(device, view, nullptr);
}
