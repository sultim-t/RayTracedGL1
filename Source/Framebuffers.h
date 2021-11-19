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

#include <array>
#include <list>
#include <vector>

#include "Common.h"
#include "CommandBufferManager.h"
#include "ISwapchainDependency.h"
#include "IFramebuffersDependency.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"
#include "Generated/ShaderCommonCFramebuf.h"

namespace RTGL1
{

// Hold info for previous and current frames
#define FRAMEBUFFERS_HISTORY_LENGTH 2

class Framebuffers
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

    bool PrepareForSize(uint32_t width, uint32_t height);

    void BarrierOne(VkCommandBuffer cmd,
                    uint32_t frameIndex,
                    FramebufferImageIndex framebufferImageIndex);

    // Barrier framebuffer images for given frameIndex 
    template <uint32_t BARRIER_COUNT>
    void BarrierMultiple(VkCommandBuffer cmd,
                         uint32_t frameIndex,
                         const FramebufferImageIndex (&framebufferImageIndices)[BARRIER_COUNT]);

    void PresentToSwapchain(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<Swapchain> &swapchain,
        FramebufferImageIndex framebufferImageIndex,
        uint32_t srcWidth, uint32_t srcHeight,
        VkImageLayout srcLayout);

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

    VkImageView GetImageView(FramebufferImageIndex framebufferImageIndex, uint32_t frameIndex) const;

    // Subscribe to framebuffers' size change event.
    // shared_ptr will be transformed to weak_ptr
    void Subscribe(std::shared_ptr<IFramebuffersDependency> subscriber);
    void Unsubscribe(const IFramebuffersDependency *subscriber);

private:
    static FramebufferImageIndex FrameIndexToFBIndex(FramebufferImageIndex framebufferImageIndex, uint32_t frameIndex);

    void CreateDescriptors();

    void CreateImages(uint32_t width, uint32_t height);
    void UpdateDescriptors();

    void DestroyImages();

    void NotifySubscribersAboutResize(uint32_t width, uint32_t height);

private:
    VkDevice device;

    std::shared_ptr<MemoryAllocator> allocator;
    std::shared_ptr<CommandBufferManager> cmdManager;
    std::shared_ptr<SamplerManager> samplerManager;

    VkExtent2D currentSize;

    std::vector<VkImage> images;
    std::vector<VkDeviceMemory> imageMemories;
    std::vector<VkImageView> imageViews;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[FRAMEBUFFERS_HISTORY_LENGTH];

    std::list<std::weak_ptr<IFramebuffersDependency>> subscribers;
};



template<uint32_t BARRIER_COUNT>
inline void Framebuffers::BarrierMultiple(VkCommandBuffer cmd, uint32_t frameIndex, const FramebufferImageIndex(&fbIndices)[BARRIER_COUNT])
{
    std::array<VkImageMemoryBarrier2KHR, BARRIER_COUNT> tmpBarriers;

    for (uint32_t i = 0; i < BARRIER_COUNT; i++)
    {
        // correct framebuf index according to the frame index
        FramebufferImageIndex fbIndex = FrameIndexToFBIndex(fbIndices[i], frameIndex);
        VkImage img = images[fbIndex];

        VkImageMemoryBarrier2KHR &b = tmpBarriers[i];
        b = {};

        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        b.image = img;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR;
        b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkImageSubresourceRange &sub = b.subresourceRange;
        sub.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        sub.baseMipLevel = 0;
        sub.levelCount = 1;
        sub.baseArrayLayer = 0;
        sub.layerCount = 1;
    }

    VkDependencyInfoKHR dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.imageMemoryBarrierCount = tmpBarriers.size();
    dependencyInfo.pImageMemoryBarriers = tmpBarriers.data();

    svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}

}