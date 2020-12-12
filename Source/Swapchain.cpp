#include "Swapchain.h"
#include "Utils.h"

Swapchain::Swapchain(VkDevice device, VkSurfaceKHR surface, std::shared_ptr<PhysicalDevice> physDevice)
{
    this->device = device;
    this->surface = surface;
    this->physDevice = physDevice;
    this->swapchain = VK_NULL_HANDLE;

    VkResult r;

    uint32_t formatCount = 0;
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice->Get(), surface, &formatCount, nullptr);
    VK_CHECKERROR(r);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(formatCount);

    r = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice->Get(), surface, &formatCount, surfaceFormats.data());
    VK_CHECKERROR(r);

    std::vector<VkFormat> acceptFormats;
    acceptFormats.push_back(VK_FORMAT_R8G8B8A8_SRGB);
    acceptFormats.push_back(VK_FORMAT_B8G8R8A8_SRGB);

    surfaceFormat = {};

    for (VkFormat f : acceptFormats)
    {
        for (VkSurfaceFormatKHR sf : surfaceFormats)
        {
            if (sf.format == f)
            {
                surfaceFormat = sf;
            }
        }

        if (surfaceFormat.format != VK_FORMAT_UNDEFINED)
        {
            break;
        }
    }

    {
        uint32_t presentModeCount = 0;
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice->Get(), surface, &presentModeCount, nullptr);
        VK_CHECKERROR(r);

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice->Get(), surface, &presentModeCount, presentModes.data());
        VK_CHECKERROR(r);

        bool foundImmediate = false;

        for (auto p : presentModes)
        {
            if (p == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                foundImmediate = true;
                break;
            }
        }

        presentModeImmediate = foundImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        presentModeVsync = VK_PRESENT_MODE_FIFO_KHR;
    }
}

void Swapchain::Recreate(std::shared_ptr<CommandBufferManager> &cmdManager, uint32_t newWidth, uint32_t newHeight, bool vsync)
{
    cmdManager->WaitDeviceIdle();

    Destroy();
    Create(cmdManager, newWidth, newHeight, vsync);
}

void Swapchain::Create(std::shared_ptr<CommandBufferManager> &cmdManager, uint32_t newWidth, uint32_t newHeight, bool vsync)
{
    CallCreateSubscribers(newWidth, newHeight);

    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice->Get(), surface, &surfCapabilities);
    VK_CHECKERROR(r);

    if (surfCapabilities.currentExtent.width != UINT32_MAX &&
        surfCapabilities.currentExtent.height != UINT32_MAX)
    {
        surfaceExtent = surfCapabilities.currentExtent;
    }
    else
    {
        surfaceExtent.width = std::min(surfCapabilities.maxImageExtent.width, newWidth);
        surfaceExtent.height = std::min(surfCapabilities.maxImageExtent.height, newHeight);

        surfaceExtent.width = std::max(surfCapabilities.minImageExtent.width, surfaceExtent.width);
        surfaceExtent.height = std::max(surfCapabilities.minImageExtent.height, surfaceExtent.height);
    }

    uint32_t imageCount = 2;
    if (surfCapabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, surfCapabilities.maxImageCount);
    }

    VkSwapchainKHR oldSwapchain = swapchain;

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = surfaceExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = surfCapabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = vsync ? presentModeVsync : presentModeImmediate;
    swapchainInfo.clipped = VK_FALSE;
    swapchainInfo.oldSwapchain = oldSwapchain;

    r = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
    VK_CHECKERROR(r);

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (VkImageView v : swapchainViews)
        {
            vkDestroyImageView(device, v, nullptr);
        }

        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    r = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    VK_CHECKERROR(r);

    swapchainImages.resize(imageCount);
    swapchainViews.resize(imageCount);

    r = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
    VK_CHECKERROR(r);

    for (uint32_t i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.components = {};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        r = vkCreateImageView(device, &viewInfo, nullptr, &swapchainViews[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, (uint64_t) swapchainImages[i], VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Swapchain image");
        SET_DEBUG_NAME(device, (uint64_t) swapchainViews[i], VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Swapchain image view");
    }

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    for (uint32_t i = 0; i < imageCount; i++)
    {
        Utils::BarrierImage(
            cmd, swapchainImages[i],
            0, 0,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    cmdManager->Submit(cmd);
    cmdManager->WaitGraphicsIdle();
}

void Swapchain::Destroy()
{
    CallDestroySubscribers();

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }

    for (VkImageView v : swapchainViews)
    {
        vkDestroyImageView(device, v, nullptr);
    }
}

void Swapchain::CallCreateSubscribers(uint32_t newWidth, uint32_t newHeight)
{
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            s->OnSwapchainCreate(newWidth, newHeight);
        }
    }
}

void Swapchain::CallDestroySubscribers()
{
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            s->OnSwapchainDestroy();
        }
    }
}

Swapchain::~Swapchain()
{
    Destroy();
}

void Swapchain::Subscribe(std::shared_ptr<ISwapchainDependency> subscriber)
{
    subscribers.emplace_back(subscriber);
}

void Swapchain::Unsubscribe(const ISwapchainDependency *subscriber)
{
    subscribers.remove_if([subscriber] (const std::weak_ptr<ISwapchainDependency> &ws)
    {
        if (const auto s = ws.lock())
        {
            return s.get() == subscriber;
        }

        return true;
    });
}
