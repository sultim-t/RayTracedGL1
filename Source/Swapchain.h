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
#include <vector>

#include "Common.h"
#include "PhysicalDevice.h"
#include "CommandBufferManager.h"
#include "ISwapchainDependency.h"

namespace RTGL1
{

class Swapchain
{
public:
    Swapchain(
        VkDevice device, 
        VkSurfaceKHR surface, 
        std::shared_ptr<PhysicalDevice> physDevice, 
        std::shared_ptr<CommandBufferManager> cmdManager);
    ~Swapchain();

    Swapchain(const Swapchain &other) = delete;
    Swapchain(Swapchain &&other) noexcept = delete;
    Swapchain &operator=(const Swapchain &other) = delete;
    Swapchain &operator=(Swapchain &&other) noexcept = delete;

    // Request new surface size. Swapchain will be recreated when the inconsitency
    // will be found. Returns true, if new extent is different from the previous one.
    // Should be called every frame change.
    bool RequestNewSize(uint32_t newWidth, uint32_t newHeight);
    bool RequestVsync(bool enable);

    void AcquireImage(VkSemaphore imageAvailableSemaphore);
    void BlitForPresent(VkCommandBuffer cmd, VkImage srcImage, uint32_t srcImageWidth, uint32_t srcImageHeight, VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_GENERAL);
    void Present(const std::shared_ptr<Queues> &queues, VkSemaphore renderFinishedSemaphore);

    // Subscribe to swapchain size chagne event.
    // shared_ptr will be transformed to weak_ptr
    void Subscribe(std::shared_ptr<ISwapchainDependency> subscriber);
    void Unsubscribe(const ISwapchainDependency *subscriber);

    VkFormat GetSurfaceFormat() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t GetCurrentImageIndex() const;
    uint32_t GetImageCount() const;
    VkImageView GetImageView(uint32_t index) const;
    const VkImageView *GetImageViews() const;

private:
    // Safe to call even if swapchain wasn't created
    bool Recreate(uint32_t newWidth, uint32_t newHeight, bool vsync);

    void Create(uint32_t newWidth, uint32_t newHeight, bool vsync, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void Destroy();
    // Destroy dresources but not the swapchain itself. Old swapchain is returned.
    VkSwapchainKHR DestroyWithoutSwapchain();

    void CallCreateSubscribers();
    void CallDestroySubscribers();

private:
    VkDevice device;
    VkSurfaceKHR surface;
    std::shared_ptr<PhysicalDevice> physDevice;
    std::shared_ptr<CommandBufferManager> cmdManager;

    VkSurfaceFormatKHR surfaceFormat;
    VkSurfaceCapabilitiesKHR surfCapabilities;
    VkPresentModeKHR presentModeVsync;
    VkPresentModeKHR presentModeImmediate;

    // user requests this extent
    VkExtent2D requestedExtent;
    bool requestedVsync;
    // current surface's size
    VkExtent2D surfaceExtent;
    bool isVsync;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;

    uint32_t currentSwapchainIndex;

    std::list<std::weak_ptr<ISwapchainDependency>> subscribers;
};

}