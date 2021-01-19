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

#include "MemoryAllocator.h"

#include "Const.h"

MemoryAllocator::MemoryAllocator(
    VkInstance instance,
    VkDevice device,
    VkPhysicalDevice physDevice)
{
    this->device = device;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.device = device;
    allocatorInfo.physicalDevice = physDevice;
    allocatorInfo.vulkanApiVersion = VK_VERSION_1_2;
    allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;

    allocatorInfo.flags =
        // currently, the library uses only one thread
        VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
        // if buffer/image requires a dedicated allocation
        VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

    VkResult r = vmaCreateAllocator(&allocatorInfo, &allocator);
    VK_CHECKERROR(r);

    CreateTexturesStagingPool();
    CreateTexturesFinalPool();
    CreateDynamicTexturesPool();
}

MemoryAllocator::~MemoryAllocator()
{
    vmaDestroyAllocator(allocator);
}

VkBuffer MemoryAllocator::CreateStagingSrcTextureBuffer(const VkBufferCreateInfo *info, VkDeviceMemory *outMemory, void **pOutMappedData)
{
    VmaAllocationCreateInfo allocInfo = {};
    // alloc TRANSFER_SRC buffer with writeable by CPU memory
    allocInfo.pool = texturesStagingPool;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer buffer;
    VmaAllocation resultAlloc;
    VmaAllocationInfo resultAllocInfo = {};

    VkResult r = vmaCreateBuffer(allocator, info, &allocInfo, &buffer, &resultAlloc, &resultAllocInfo);
    VK_CHECKERROR(r);

    if (buffer == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    bufAllocs[buffer] = resultAlloc;

    *outMemory = resultAllocInfo.deviceMemory;
    *pOutMappedData = resultAllocInfo.pMappedData;
    return buffer;
}

VkImage MemoryAllocator::CreateDstTextureImage(const VkImageCreateInfo *info, VkDeviceMemory *outMemory)
{
    VmaAllocationCreateInfo allocInfo = {};
    // alloc SAMPLED_BIT | TRANSFER_DST
    allocInfo.pool = texturesFinalPool;
    allocInfo.flags = 0;

    VkImage image;
    VmaAllocation resultAlloc;
    VmaAllocationInfo resultAllocInfo = {};

    VkResult r = vmaCreateImage(allocator, info, &allocInfo, &image, &resultAlloc, &resultAllocInfo);
    VK_CHECKERROR(r);

    if (image == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    imgAllocs[image] = resultAlloc;

    *outMemory = resultAllocInfo.deviceMemory;
    return image;
}

VkImage MemoryAllocator::CreateDynamicTextureImage(const VkImageCreateInfo *info, VkDeviceMemory *outMemory, void **pOutMappedData)
{
    VmaAllocationCreateInfo allocInfo = {};
    // alloc SAMPLED_BIT image with writeable by CPU memory
    allocInfo.pool = dynamicTexturesPool;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkImage image;
    VmaAllocation resultAlloc;
    VmaAllocationInfo resultAllocInfo = {};

    VkResult r = vmaCreateImage(allocator, info, &allocInfo, &image, &resultAlloc, &resultAllocInfo);
    VK_CHECKERROR(r);

    if (image == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    imgAllocs[image] = resultAlloc;

    *outMemory = resultAllocInfo.deviceMemory;
    *pOutMappedData = resultAllocInfo.pMappedData;
    return image;
}

void MemoryAllocator::DestroyStagingSrcTextureBuffer(VkBuffer buffer)
{
    if (bufAllocs.find(buffer) == bufAllocs.end())
    {
        assert(0);
        return;
    }

    vmaDestroyBuffer(allocator, buffer, bufAllocs[buffer]);
    bufAllocs.erase(buffer);
}

void MemoryAllocator::DestroyTextureImage(VkImage image)
{
    if (imgAllocs.find(image) == imgAllocs.end())
    {
        assert(0);
        return;
    }

    vmaDestroyImage(allocator, image, imgAllocs[image]);
    imgAllocs.erase(image);
}

void MemoryAllocator::CreateTexturesStagingPool()
{
    VkResult r;

    // Vma will create and destroy it for identifying the memory type index 
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 64;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // transfer source, will be filled from the cpu
    VmaAllocationCreateInfo prototype = {};
    prototype.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    uint32_t memTypeIndex;
    r = vmaFindMemoryTypeIndexForBufferInfo(allocator, &bufferInfo, &prototype, &memTypeIndex);
    VK_CHECKERROR(r);

    VmaPoolCreateInfo poolInfo = {};
    poolInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
    poolInfo.memoryTypeIndex = memTypeIndex;
    poolInfo.blockSize = ALLOCATOR_BLOCK_SIZE_STATIC_STAGING_TEXTURES;
    // buddy algorithm as textures has commonly a size of power of 2
    poolInfo.flags = VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT;

    r = vmaCreatePool(allocator, &poolInfo, &texturesStagingPool);
    VK_CHECKERROR(r);
}

void MemoryAllocator::CreateTexturesFinalPool()
{
    VkResult r;

    // Vma will create and destroy it for identifying the memory type index 
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.extent.width = 1;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    // transfer destination, data will be copied from staging buffer
    VmaAllocationCreateInfo prototype = {};
    prototype.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    uint32_t memTypeIndex;
    r = vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &prototype, &memTypeIndex);
    VK_CHECKERROR(r);

    VmaPoolCreateInfo poolInfo = {};
    poolInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
    poolInfo.memoryTypeIndex = memTypeIndex;
    poolInfo.blockSize = ALLOCATOR_BLOCK_SIZE_STATIC_STAGING_TEXTURES;
    // buddy algorithm as textures has commonly a size of power of 2
    poolInfo.flags = VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT;

    r = vmaCreatePool(allocator, &poolInfo, &texturesFinalPool);
    VK_CHECKERROR(r);
}

void MemoryAllocator::CreateDynamicTexturesPool()
{
    VkResult r;

    // Vma will create and destroy it for identifying the memory type index 
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.extent.width = 1;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    // memory will be written by CPU and read by GPU
    VmaAllocationCreateInfo prototype = {};
    prototype.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    uint32_t memTypeIndex;
    r = vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &prototype, &memTypeIndex);
    VK_CHECKERROR(r);

    VmaPoolCreateInfo poolInfo = {};
    poolInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
    poolInfo.memoryTypeIndex = memTypeIndex;
    poolInfo.blockSize = ALLOCATOR_BLOCK_SIZE_DYNAMIC_TEXTURES;
    // buddy algorithm as textures has commonly a size of power of 2
    poolInfo.flags = VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT;

    r = vmaCreatePool(allocator, &poolInfo, &dynamicTexturesPool);
    VK_CHECKERROR(r);
}

