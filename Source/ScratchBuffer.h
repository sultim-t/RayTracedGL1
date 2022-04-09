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

#include <list>

#include "Buffer.h"

namespace RTGL1
{

class ScratchBuffer
{
public:
    explicit ScratchBuffer(std::shared_ptr<MemoryAllocator> allocator, uint32_t alignment = 1);

    ScratchBuffer(const ScratchBuffer& other) = delete;
    ScratchBuffer(ScratchBuffer&& other) noexcept = delete;
    ScratchBuffer& operator=(const ScratchBuffer& other) = delete;
    ScratchBuffer& operator=(ScratchBuffer&& other) noexcept = delete;

    // get scratch buffer address
    VkDeviceAddress GetScratchAddress(VkDeviceSize scratchSize);
    void Reset();

private:
    void AddChunk(VkDeviceSize size);

private:
    struct ChunkBuffer
    {
        Buffer buffer;
        uint32_t currentOffset = 0;
    };

    std::weak_ptr<MemoryAllocator> allocator;
    std::list<ChunkBuffer> chunks;
    uint32_t alignment = 1;
};

}