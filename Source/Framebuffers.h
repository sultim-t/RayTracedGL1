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

#include <vector>

#include "Common.h"
#include "CommandBufferManager.h"
#include "ISwapchainDependency.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"
#include "Generated/ShaderCommonCFramebuf.h"

namespace RTGL1
{

// Hold info for previous and current frames
#define FRAMEBUFFERS_HISTORY_LENGTH 2

class Framebuffers : public ISwapchainDependency
{
public:
    explicit Framebuffers(VkDevice device,
                          std::shared_ptr<MemoryAllocator> allocator,
                          std::shared_ptr<CommandBufferManager> cmdManager,
                          std::shared_ptr<SamplerManager> samplerManager);
    ~Framebuffers();

    Framebuffers(const Framebuffers &other) = delete;
    Framebuffers(Framebuffers &&other) noexcept = delete;
    Framebuffers &operator=(const Framebuffers &other) = delete;
    Framebuffers &operator=(Framebuffers &&other) noexcept = delete;

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;

    void Barrier(
        VkCommandBuffer cmd,
        uint32_t frameIndex,
        FramebufferImageIndex framebufferImageIndex);

    void PresentToSwapchain(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<Swapchain> &swapchain,
        FramebufferImageIndex framebufferImageIndex,
        uint32_t srcWidth, uint32_t srcHeight,
        VkImageLayout srcLayout);

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

private:
    void CreateDescriptors();

    void CreateImages(uint32_t width, uint32_t height);
    void UpdateDescriptors();

    void DestroyImages();

private:
    VkDevice device;

    std::shared_ptr<MemoryAllocator> allocator;
    std::shared_ptr<CommandBufferManager> cmdManager;
    std::shared_ptr<SamplerManager> samplerManager;

    std::vector<VkImage> images;
    std::vector<VkDeviceMemory> imageMemories;
    std::vector<VkImageView> imageViews;

    std::vector<VkImageMemoryBarrier> barriers;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[FRAMEBUFFERS_HISTORY_LENGTH];
};

}