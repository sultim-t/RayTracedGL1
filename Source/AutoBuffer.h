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

#pragma once

#include "Buffer.h"
#include "MemoryAllocator.h"

namespace RTGL1
{

// This class encapsulate staging buffers for each frame in flight 
// and one device local buffer to copy in.
class AutoBuffer
{
public:
    AutoBuffer(
        VkDevice device,
        std::shared_ptr<MemoryAllocator> allocator, 
        const char *debugNameStaging = nullptr,
        const char *debugName = nullptr);
    ~AutoBuffer();

    AutoBuffer(const AutoBuffer &other) = delete;
    AutoBuffer(AutoBuffer &&other) noexcept = delete;
    AutoBuffer &operator=(const AutoBuffer &other) = delete;
    AutoBuffer &operator=(AutoBuffer &&other) noexcept = delete;

    void Create(VkDeviceSize size, VkBufferUsageFlags usage);
    void Destroy();

    void CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    VkBuffer GetStaging(uint32_t frameIndex);
    void *GetMapped(uint32_t frameIndex);

    VkBuffer GetDeviceLocal();
    VkDeviceAddress GetDeviceAddress();

    VkDeviceSize GetSize() const;

private:
    VkDevice device;
    std::shared_ptr<MemoryAllocator> allocator;

    Buffer staging[MAX_FRAMES_IN_FLIGHT];
    Buffer deviceLocal;

    void *mapped[MAX_FRAMES_IN_FLIGHT];

    const char *debugNameStaging;
    const char *debugName;
};

}