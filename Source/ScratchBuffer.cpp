#include "ScratchBuffer.h"

constexpr VkDeviceSize SCRATCH_CHUNK_BUFFER_SIZE = (1 << 24);

ScratchBuffer::ScratchBuffer(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice)
{
    this->device = device;
    this->physDevice = physDevice;

    AddChunk(SCRATCH_CHUNK_BUFFER_SIZE);
}

VkDeviceAddress ScratchBuffer::GetScratchAddress(VkDeviceSize scratchSize)
{
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

void ScratchBuffer::AddChunk(VkDeviceSize size)
{
    if (const auto pd = physDevice.lock())
    {
        chunks.emplace_back();
        auto &c = chunks.back();

        c.buffer.Init(
            device, *pd, size,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "Scratch buffer");
    }
}
