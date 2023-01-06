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

#include <algorithm>

#include "Utils.h"

using namespace RTGL1;

constexpr VkDeviceSize SCRATCH_CHUNK_BUFFER_SIZE = (1 << 24);

ScratchBuffer::ScratchBuffer(std::shared_ptr<MemoryAllocator> _allocator, uint32_t _alignment)
:
    allocator(_allocator),
    alignment(_alignment)
{
    AddChunk(SCRATCH_CHUNK_BUFFER_SIZE);
}

VkDeviceAddress ScratchBuffer::GetScratchAddress(VkDeviceSize scratchSize)
{
    // the fastest way to always return an aligned address is simply to align all allocation sizes
    const VkDeviceSize alignedSize = Utils::Align(scratchSize, (VkDeviceSize)alignment);

    // find chunk with appropriate size
    for (auto &c : chunks)
    {
        if (alignedSize < c.buffer.GetSize() - c.currentOffset)
        {
            VkDeviceAddress address = c.buffer.GetAddress() + c.currentOffset;

            c.currentOffset += alignedSize;
            return address;
        }
    }

    // couldn't find chunk, create new one
    AddChunk(std::max(SCRATCH_CHUNK_BUFFER_SIZE, alignedSize));
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
            *allc, size,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "Scratch buffer");
    }
}
