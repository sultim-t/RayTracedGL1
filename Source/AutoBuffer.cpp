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

RTGL1::AutoBuffer::AutoBuffer(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> _allocator,
    const char *_debugNameStaging, 
    const char *_debugName)
:
    device(_device),
    allocator(std::move(_allocator)),
    mapped{},
    debugNameStaging(_debugNameStaging),
    debugName(_debugName)
{}

RTGL1::AutoBuffer::~AutoBuffer()
{
    Destroy();
}

void RTGL1::AutoBuffer::Create(VkDeviceSize size, VkBufferUsageFlags usage)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(!staging[i].IsInitted());

        staging[i].Init(
            allocator, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            debugNameStaging);

        mapped[i] = staging[i].Map();
    }

    assert(!deviceLocal.IsInitted());

    deviceLocal.Init(
        allocator, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        debugName);
}

void RTGL1::AutoBuffer::Destroy()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (staging[i].IsInitted());
        {
            staging[i].Unmap();
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
    assert(offset + size < staging[frameIndex].GetSize());
    assert(offset + size < deviceLocal.GetSize());

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
}

void *RTGL1::AutoBuffer::GetMapped(uint32_t frameIndex)
{
    assert(staging[frameIndex].IsMapped());
    return mapped[frameIndex];
}

VkBuffer RTGL1::AutoBuffer::GetStaging(uint32_t frameIndex)
{
    assert(staging[frameIndex].IsInitted());
    return staging[frameIndex].GetBuffer();
}

VkBuffer RTGL1::AutoBuffer::GetDeviceLocal()
{
    assert(deviceLocal.IsInitted());
    return deviceLocal.GetBuffer();
}
