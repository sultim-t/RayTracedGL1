#include "ScratchBuffer.h"

constexpr VkDeviceSize SCRATCH_CHUNK_BUFFER_SIZE = (1 << 24);

ScratchBuffer::ScratchBuffer(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice)
{
    this->device = device;
    this->physDevice = physDevice;

    AddChunk(SCRATCH_CHUNK_BUFFER_SIZE);
}

VkDeviceAddress ScratchBuffer::GetScratchAddress(VkAccelerationStructureKHR as, bool update)
{
    VkDeviceSize scratchSize = GetScratchSize(as, update);

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

    // couldn't find chunk, create new one
    AddChunk(std::max(SCRATCH_CHUNK_BUFFER_SIZE, scratchSize));
    return chunks.back().buffer.GetAddress();
}

void ScratchBuffer::Reset()
{
    for (auto &c : chunks)
    {
        c.currentOffset = 0;
    }
}

VkDeviceSize ScratchBuffer::GetScratchSize(VkAccelerationStructureKHR as, bool update) const
{
    VkAccelerationStructureMemoryRequirementsInfoKHR scratchMemReqInfo = {};
    scratchMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    scratchMemReqInfo.type = update ?
        VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_KHR :
        VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
    scratchMemReqInfo.accelerationStructure = as;
    scratchMemReqInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

    VkMemoryRequirements2 memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vksGetAccelerationStructureMemoryRequirementsKHR(device, &scratchMemReqInfo, &memReq2);

    return memReq2.memoryRequirements.size;
}

void ScratchBuffer::AddChunk(VkDeviceSize size)
{
    if (const auto pd = physDevice.lock())
    {
        chunks.emplace_back();
        auto &c = chunks.back();

        c.buffer.Init(device, *pd, size,
                      VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}
