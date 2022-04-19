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

#include "AutoBuffer.h"

RTGL1::AutoBuffer::AutoBuffer(std::shared_ptr<MemoryAllocator> _allocator)
:
    allocator(std::move(_allocator)),
    mapped{}
{}

// TODO: remove this constructor
RTGL1::AutoBuffer::AutoBuffer(VkDevice _device, std::shared_ptr<MemoryAllocator> _allocator) : AutoBuffer(std::move(_allocator))
{}

RTGL1::AutoBuffer::~AutoBuffer()
{
    Destroy();
}

void RTGL1::AutoBuffer::Create(VkDeviceSize size, VkBufferUsageFlags usage, const std::string &debugName, uint32_t frameCount)
{
    assert(frameCount > 0 && frameCount <= MAX_FRAMES_IN_FLIGHT);

    const std::string debugNameStaging = debugName + " - staging";

    for (uint32_t i = 0; i < frameCount; i++)
    {
        assert(!staging[i].IsInitted());

        staging[i].Init(
            allocator, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            debugNameStaging.c_str());

        mapped[i] = staging[i].Map();
    }

    assert(!deviceLocal.IsInitted());

    deviceLocal.Init(
        allocator, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        debugName.c_str());
}

void RTGL1::AutoBuffer::Destroy()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (staging[i].IsInitted())
        {
            staging[i].TryUnmap();
            staging[i].Destroy();
        }

        mapped[i] = nullptr;
    }

    if (deviceLocal.IsInitted());
    {
        deviceLocal.Destroy();
    }
}

void RTGL1::AutoBuffer::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, VkDeviceSize size, VkDeviceSize offset)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    assert(staging[frameIndex].GetSize() == deviceLocal.GetSize());

    if (size == VK_WHOLE_SIZE)
    {
        size = deviceLocal.GetSize();
    }
    else
    {
        assert(offset + size <= staging[frameIndex].GetSize());
        assert(offset + size <= deviceLocal.GetSize());
    }

    if (size == 0)
    {
        return;
    }

    VkBufferCopy info = {};
    info.srcOffset = offset;
    info.dstOffset = offset;
    info.size = size;

    vkCmdCopyBuffer(
        cmd,
        staging[frameIndex].GetBuffer(), deviceLocal.GetBuffer(),
        1, &info);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = deviceLocal.GetBuffer();
    barrier.offset = offset;
    barrier.size = size;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void RTGL1::AutoBuffer::CopyFromStaging(
    VkCommandBuffer cmd, uint32_t frameIndex, 
    const VkBufferCopy *copyInfos, uint32_t copyInfosCount)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    assert(staging[frameIndex].GetSize() == deviceLocal.GetSize());

    if (copyInfosCount == 0)
    {
        return;
    }

    vkCmdCopyBuffer(
        cmd,
        staging[frameIndex].GetBuffer(), deviceLocal.GetBuffer(),
        copyInfosCount, copyInfos);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = deviceLocal.GetBuffer();

    for (uint32_t i = 0; i < copyInfosCount; ++i)
    {
        barrier.offset = copyInfos[i].dstOffset;
        barrier.size = copyInfos[i].size;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }
}

void *RTGL1::AutoBuffer::GetMapped(uint32_t frameIndex)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    assert(staging[frameIndex].IsMapped());
    return mapped[frameIndex];
}

VkBuffer RTGL1::AutoBuffer::GetStaging(uint32_t frameIndex)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    assert(staging[frameIndex].IsInitted());
    return staging[frameIndex].GetBuffer();
}

VkBuffer RTGL1::AutoBuffer::GetDeviceLocal()
{
    assert(deviceLocal.IsInitted());
    return deviceLocal.GetBuffer();
}

VkDeviceAddress RTGL1::AutoBuffer::GetDeviceAddress()
{
    return deviceLocal.GetAddress();
}

VkDeviceSize RTGL1::AutoBuffer::GetSize() const
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(deviceLocal.GetSize() == staging[i].GetSize());
    }

    return deviceLocal.GetSize();
}
