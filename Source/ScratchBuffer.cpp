#include "ScratchBuffer.h"

#define SCRATCH_CHUNK_BUFFER_SIZE (1 << 24)

ScratchBuffer::ScratchBuffer(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice)
{
    this->device = device;
    this->physDevice = physDevice;

    AddChunk();
}

VkDeviceAddress ScratchBuffer::GetScratchAddress(VkAccelerationStructureKHR as)
{
    VkDeviceSize scratchSize = GetScratchSize(as);

    // find chunk with appropriate size
    for (auto &c : chunks)
    {
        if (scratchSize < c.buffer.GetSize() - c.currentOffset)
        {
            VkDeviceAddress address = c.buffer.GetAddress() + c.currentOffset;

            c.currentOffset += scratchSize;
            return address;
        }
    }

    assert(0 && "SCRATCH_CHUNK_BUFFER_SIZE must be increased");
    return 0;
}

void ScratchBuffer::Reset()
{
    for (auto &c : chunks)
    {
        c.currentOffset = 0;
    }
}

VkDeviceSize ScratchBuffer::GetScratchSize(VkAccelerationStructureKHR as) const
{
    VkAccelerationStructureMemoryRequirementsInfoKHR scratchMemReqInfo = {};
    scratchMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    scratchMemReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
    scratchMemReqInfo.accelerationStructure = as;
    scratchMemReqInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

    VkMemoryRequirements2 memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vksGetAccelerationStructureMemoryRequirementsKHR(device, &scratchMemReqInfo, &memReq2);

    return memReq2.memoryRequirements.size;
}

void ScratchBuffer::AddChunk()
{
    if (const auto pd = physDevice.lock())
    {
        chunks.emplace_back();
        auto &c = chunks.back();

        c.buffer.Init(device, *pd, SCRATCH_CHUNK_BUFFER_SIZE,
                      VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}
