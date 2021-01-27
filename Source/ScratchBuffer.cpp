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

#include "ScratchBuffer.h"

constexpr VkDeviceSize SCRATCH_CHUNK_BUFFER_SIZE = (1 << 24);

ScratchBuffer::ScratchBuffer(std::shared_ptr<MemoryAllocator> _allocator)
    : allocator(_allocator)
{
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
    if (const auto allc = allocator.lock())
    {
        chunks.emplace_back();
        auto &c = chunks.back();

        c.buffer.Init(
            allc, size,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "Scratch buffer");
    }
}
