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

#include "CubemapUploader.h"

RTGL1::CubemapUploader::CubemapUploader(VkDevice device, std::shared_ptr<MemoryAllocator> memAllocator)
    :TextureUploader(device, std::move(memAllocator))
{}

RTGL1::TextureUploader::UploadResult RTGL1::CubemapUploader::UploadImage(const UploadInfo &info)
{
    assert(info.isCubemap);

    // cubemaps can't be dynamic
    assert(!info.isDynamic);

    const RgExtent2D &size = info.baseSize;

    UploadResult result = {};
    result.wasUploaded = false;

    VkImage image;

    VkBuffer stagingBuffers[6] = {};
    void *mappedData[6] = {};

    // 1. Allocate and fill buffer
    const uint32_t faceNumber = 6;
    VkDeviceSize faceSize = (VkDeviceSize)info.dataSize;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = faceSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    for (uint32_t i = 0; i < 6; i++)
    {
        stagingBuffers[i] = memAllocator->CreateStagingSrcTextureBuffer(&stagingInfo, info.pDebugName, &mappedData[i]);

        // if couldn't allocate memory
        if (stagingBuffers[i] == VK_NULL_HANDLE)
        {
            // clear allocated
            for (uint32_t j = 0; j < i; j++)
            {
                memAllocator->DestroyStagingSrcTextureBuffer(stagingBuffers[j]);
            }

            return result;
        }

        SET_DEBUG_NAME(device, stagingBuffers[i], VK_OBJECT_TYPE_BUFFER, info.pDebugName);
    }


    bool wasCreated = CreateImage(info, &image);
    if (!wasCreated)
    {
        // clean created resources
        for (uint32_t j = 0; j < 6; j++)
        {
            memAllocator->DestroyStagingSrcTextureBuffer(stagingBuffers[j]);
        }

        return result;
    }

    // copy image data to buffer
    for (uint32_t i = 0; i < 6; i++)
    {
        memcpy(mappedData[i], info.cubemap.pFaces[i], faceSize);
    }


    // and copy it to image
    PrepareImage(image, stagingBuffers, info, ImagePrepareType::INIT);

    // create image view
    VkImageView imageView = CreateImageView(image, info.format, info.isCubemap, GetMipmapCount(size, info));

    SET_DEBUG_NAME(device, imageView, VK_OBJECT_TYPE_IMAGE_VIEW, info.pDebugName);


    // push staging buffer to be deleted when it won't be in use
    for (uint32_t i = 0; i < 6; i++)
    {
        stagingToFree[info.frameIndex].push_back(stagingBuffers[i]);
    }

    // return results
    result.wasUploaded = true;
    result.image = image;
    result.view = imageView;
    return result;
}
