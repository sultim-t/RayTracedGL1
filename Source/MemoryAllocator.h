#pragma once

#include <map>

#include "Common.h"
#include "Vma/vk_mem_alloc.h"

// Device memory allocator.
class MemoryAllocator
{
public:
    explicit MemoryAllocator(
        VkInstance instance,
        VkDevice device,
        VkPhysicalDevice physDevice);
    ~MemoryAllocator();

    MemoryAllocator(const MemoryAllocator &other) = delete;
    MemoryAllocator(MemoryAllocator &&other) noexcept = delete;
    MemoryAllocator &operator=(const MemoryAllocator &other) = delete;
    MemoryAllocator &operator=(MemoryAllocator &&other) noexcept = delete;

    VkBuffer CreateStagingSrcTextureBuffer(
        const VkBufferCreateInfo *info, 
        VkDeviceMemory *outMemory, void **pOutMappedData);
    VkImage CreateDstTextureImage(
        const VkImageCreateInfo *info,
        VkDeviceMemory *outMemory);
    VkImage CreateDynamicTextureImage(
        const VkImageCreateInfo *info,
        VkDeviceMemory *outMemory, void **pOutMappedData);

    void DestroyStagingSrcTextureBuffer(VkBuffer buffer);
    void DestroyTextureImage(VkImage image);

private:
    void CreateTexturesStagingPool();
    void CreateTexturesFinalPool();
    void CreateDynamicTexturesPool();

private:
    VkDevice device;
    VmaAllocator allocator;

    // pool for staging buffers for static texture data, CPU_ONLY
    VmaPool texturesStagingPool;
    // pool for images, GPU_ONLY
    // texture data will be copied from staging to this memory
    VmaPool texturesFinalPool;

    // pool for dynamic textures that will be updated frequently, CPU_TO_GPU
    VmaPool dynamicTexturesPool;

    // maps for freeing corresponding allocations
    std::map<VkBuffer, VmaAllocation> bufAllocs;
    std::map<VkImage, VmaAllocation> imgAllocs;
};