#pragma once

#include <list>
#include <vector>
#include "Common.h"
#include "PhysicalDevice.h"
#include "CommandBufferManager.h"

class Swapchain
{
public:
    class ISwapchainDependency
    {
    public:
        virtual ~ISwapchainDependency() = default;
        virtual void OnSwapchainCreate(uint32_t newWidth, uint32_t newHeight) = 0;
        virtual void OnSwapchainDestroy() = 0;
    };

public:
    Swapchain(VkDevice device, VkSurfaceKHR surface, std::shared_ptr<PhysicalDevice> physDevice);
    ~Swapchain();

    Swapchain(const Swapchain &other) = delete;
    Swapchain(Swapchain &&other) noexcept = delete;
    Swapchain &operator=(const Swapchain &other) = delete;
    Swapchain &operator=(Swapchain &&other) noexcept = delete;

    void SetWindowSize(uint32_t imageWidth, uint32_t imageHeight);

    void AcquireImage(VkSemaphore imageAvailableSemaphore);
    void BlitForPresent(VkCommandBuffer cmd, VkImage srcImage, uint32_t imageWidth, uint32_t imageHeight, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL);
    void Present(const Queues &queues, VkSemaphore renderFinishedSemaphore);

    void Subscribe(std::shared_ptr<ISwapchainDependency> subscriber);
    void Unsubscribe(const ISwapchainDependency *subscriber);

private:
    void Recreate(std::shared_ptr<CommandBufferManager> &cmdManager, uint32_t newWidth, uint32_t newHeight, bool vsync);

    void Create(std::shared_ptr<CommandBufferManager> &cmdManager, uint32_t newWidth, uint32_t newHeight, bool vsync);
    void Destroy();

    void CallCreateSubscribers(uint32_t newWidth, uint32_t newHeight);
    void CallDestroySubscribers();

private:
    VkDevice device;
    std::shared_ptr<PhysicalDevice> physDevice;
    VkSurfaceKHR surface;

    VkSurfaceFormatKHR surfaceFormat;
    VkSurfaceCapabilitiesKHR surfCapabilities;
    VkPresentModeKHR presentModeVsync;
    VkPresentModeKHR presentModeImmediate;

    VkExtent2D surfaceExtent;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;

    uint32_t currentSwapchainIndex;

    std::list<std::weak_ptr<ISwapchainDependency>> subscribers;
};