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

#include "Common.h"
#include "MemoryAllocator.h"

namespace RTGL1
{

class Buffer
{
public:
    Buffer();
    ~Buffer();

    // Create VkBuffer, allocate memory and bind it
    void Init(const std::shared_ptr<MemoryAllocator> &allocator,
              VkDeviceSize size, VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties, const char *debugName = nullptr);
    void Destroy();

    void* Map();
    void Unmap();
    bool TryUnmap();


    VkBuffer GetBuffer() const;
    VkDeviceMemory GetMemory() const;
    // To get address usage flags must contain VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    VkDeviceAddress GetAddress() const;
    VkDeviceSize GetSize() const;
    bool IsMapped() const;
    bool IsInitted() const;

protected:
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    VkDeviceSize size;

private:
    bool isMapped;
};

}