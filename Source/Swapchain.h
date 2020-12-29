#pragma once

#include <list>
#include <vector>
#include "Common.h"
#include "PhysicalDevice.h"
#include "CommandBufferManager.h"
#include "ISwapchainDependency.h"

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

private:
    // Safe to call even if swapchain wasn't created
    bool Recreate(uint32_t newWidth, uint32_t newHeight, bool vsync);

    void Create(uint32_t newWidth, uint32_t newHeight, bool vsync);
    void Destroy();

    void CallCreateSubscribers(uint32_t newWidth, uint32_t newHeight);
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