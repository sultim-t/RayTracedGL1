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

#pragma once

#include <map>

#include "Common.h"
#include "PhysicalDevice.h"
#include "Vma/vk_mem_alloc.h"

namespace RTGL1
{

// Device memory allocator.
class MemoryAllocator
{
public:
    explicit MemoryAllocator(
        VkInstance instance,
        VkDevice device,
        std::shared_ptr<PhysicalDevice> physDevice);
    ~MemoryAllocator();

    MemoryAllocator(const MemoryAllocator &other) = delete;
    MemoryAllocator(MemoryAllocator &&other) noexcept = delete;
    MemoryAllocator &operator=(const MemoryAllocator &other) = delete;
    MemoryAllocator &operator=(MemoryAllocator &&other) noexcept = delete;

    VkDevice GetDevice();


    // If addressQuery=true device address can be queried
    VkDeviceMemory AllocDedicated(const VkMemoryRequirements &memReqs, VkMemoryPropertyFlags properties, bool addressQuery = false) const;
    VkDeviceMemory AllocDedicated(const VkMemoryRequirements2 &memReqs2, VkMemoryPropertyFlags properties, bool addressQuery = false) const;
    void FreeDedicated(VkDeviceMemory memory) const;

    
    VkBuffer CreateStagingSrcTextureBuffer(
        const VkBufferCreateInfo *info, 
        void **pOutMappedData, VkDeviceMemory *outMemory = nullptr);
    VkImage CreateDstTextureImage(
        const VkImageCreateInfo *info,
        VkDeviceMemory *outMemory = nullptr);

    void DestroyStagingSrcTextureBuffer(VkBuffer buffer);
    void DestroyTextureImage(VkImage image);

private:
    void CreateTexturesStagingPool();
    void CreateTexturesFinalPool();

private:
    VkDevice device;
    std::shared_ptr<PhysicalDevice> physDevice;

    VmaAllocator allocator;

    // pool for staging buffers for texture data, CPU_ONLY
    VmaPool texturesStagingPool;
    // pool for images, GPU_ONLY
    // texture data will be copied from staging to this memory
    VmaPool texturesFinalPool;

    // maps for freeing corresponding allocations
    std::map<VkBuffer, VmaAllocation> bufAllocs;
    std::map<VkImage, VmaAllocation> imgAllocs;
};

}