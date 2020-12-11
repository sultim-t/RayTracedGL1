#pragma once

#include "Buffer.h"
#include <list>

class ScratchBuffer
{
public:
    ScratchBuffer(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice);

    // get scratch buffer address for given acceleration structure
    VkDeviceAddress GetScratchAddress(VkAccelerationStructureKHR as);
    void Reset();

private:
    VkDeviceSize GetScratchSize(VkAccelerationStructureKHR as) const;
    void AddChunk();

private:
    struct ChunkBuffer
    {
        Buffer buffer;
        uint32_t currentOffset = 0;
    };

    VkDevice device;

    std::weak_ptr<PhysicalDevice> physDevice;
    std::list<ChunkBuffer> chunks;
};