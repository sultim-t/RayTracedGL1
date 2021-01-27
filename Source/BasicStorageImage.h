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
#include "CommandBufferManager.h"
#include "MemoryAllocator.h"
#include "ISwapchainDependency.h"

// Temporary class for simple storing of a ray traced image 
class BasicStorageImage final : public ISwapchainDependency
{
public:
    explicit BasicStorageImage(VkDevice device,
                      std::shared_ptr<MemoryAllocator> allocator,
                      std::shared_ptr<CommandBufferManager> cmdManager);
    ~BasicStorageImage() override;

    BasicStorageImage(const BasicStorageImage &other) = delete;
    BasicStorageImage(BasicStorageImage &&other) noexcept = delete;
    BasicStorageImage &operator=(const BasicStorageImage &other) = delete;
    BasicStorageImage &operator=(BasicStorageImage &&other) noexcept = delete;

    void Barrier(VkCommandBuffer cmd);

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const
    {
        return descSets[frameIndex];
    }

    VkDescriptorSetLayout GetDescSetLayout() const
    {
        return descLayout;
    }

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;

private:
    void CreateImage(uint32_t width, uint32_t height);
    void DestroyImage();

    void CreateDescriptors();
    void UpdateDescriptors();

public:
    VkImage image;
    VkImageLayout imageLayout;
    uint32_t width, height;

private:
    VkDevice device;
    std::shared_ptr<MemoryAllocator> allocator;
    std::shared_ptr<CommandBufferManager> cmdManager;

    VkImageView view;
    VkDeviceMemory memory;

    VkDescriptorSetLayout descLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];
};

