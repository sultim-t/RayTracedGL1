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

#include "Swapchain.h"
#include "Utils.h"

using namespace RTGL1;

Swapchain::Swapchain(VkDevice device, VkSurfaceKHR surface,
                     std::shared_ptr<PhysicalDevice> physDevice,
                     std::shared_ptr<CommandBufferManager> cmdManager) :
    surfaceFormat{},
    surfCapabilities{},
    // default
    presentModeVsync(VK_PRESENT_MODE_FIFO_KHR),
    presentModeImmediate(VK_PRESENT_MODE_FIFO_KHR),
    requestedExtent({ 0, 0 }),
    requestedVsync(true),
    surfaceExtent{ UINT32_MAX, UINT32_MAX },
    isVsync(true),
    swapchain(VK_NULL_HANDLE),
    currentSwapchainIndex(UINT32_MAX)
{
    this->device = device;
    this->surface = surface;
    this->physDevice = physDevice;
    this->cmdManager = cmdManager;

    VkResult r;

    // find surface format
    {
        uint32_t formatCount = 0;
        r = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice->Get(), surface, &formatCount, nullptr);
        VK_CHECKERROR(r);

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        surfaceFormats.resize(formatCount);

        r = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice->Get(), surface, &formatCount, surfaceFormats.data());
        VK_CHECKERROR(r);

        std::vector<VkFormat> acceptFormats =
        {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB
        };

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
    }

    // find present modes
    {
        uint32_t presentModeCount = 0;
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice->Get(), surface, &presentModeCount, nullptr);
        VK_CHECKERROR(r);

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice->Get(), surface, &presentModeCount, presentModes.data());
        VK_CHECKERROR(r);

        for (auto p : presentModes)
        {
            // try to find mailbox
            if (p == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentModeImmediate = p;
            }

            if (p == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            {
                presentModeVsync = p;
            }
        }
    }
}

bool Swapchain::RequestNewSize(uint32_t newWidth, uint32_t newHeight)
{
    requestedExtent.width = newWidth;
    requestedExtent.height = newHeight;

    return requestedExtent.width != surfaceExtent.width
        || requestedExtent.height != surfaceExtent.height;
}

bool Swapchain::RequestVsync(bool enable)
{
    requestedVsync = enable;
    return requestedVsync != isVsync;
}

void Swapchain::AcquireImage(VkSemaphore imageAvailableSemaphore)
{
    // if requested params are different
    if (requestedExtent.width != surfaceExtent.width || 
        requestedExtent.height != surfaceExtent.height || 
        requestedVsync != isVsync)
    {
        Recreate(requestedExtent.width, requestedExtent.height, requestedVsync);
    }

    while (true)
    {
        VkResult r = vkAcquireNextImageKHR(
            device, swapchain, UINT64_MAX,
            imageAvailableSemaphore,
            VK_NULL_HANDLE, &currentSwapchainIndex);

        if (r == VK_SUCCESS)
        {
            break;
        }
        else if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        {
            Recreate(requestedExtent.width, requestedExtent.height, requestedVsync);
        }
        else
        {
            assert(0);
        }
    }
}

void Swapchain::BlitForPresent(VkCommandBuffer cmd, VkImage srcImage, uint32_t srcImageWidth,
                               uint32_t srcImageHeight, VkImageLayout srcImageLayout)
{
    VkImageBlit region = {};

    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.srcOffsets[0] = { 0, 0, 0 };
    region.srcOffsets[1] = { static_cast<int32_t>(srcImageWidth), static_cast<int32_t>(srcImageHeight), 1 };

    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstOffsets[0] = { 0, 0, 0 };
    region.dstOffsets[1] = { static_cast<int32_t>(surfaceExtent.width), static_cast<int32_t>(surfaceExtent.height), 1 };

    VkImage swapchainImage = swapchainImages[currentSwapchainIndex];
    VkImageLayout swapchainImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // set layout for blit
    Utils::BarrierImage(
        cmd, srcImage,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Utils::BarrierImage(
        cmd, swapchainImage,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        swapchainImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdBlitImage(
        cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);

    // restore layouts
    Utils::BarrierImage(
        cmd, srcImage,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcImageLayout);

    Utils::BarrierImage(
        cmd, swapchainImage,
        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, swapchainImageLayout);
}

void Swapchain::Present(const std::shared_ptr<Queues> &queues, VkSemaphore renderFinishedSemaphore)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentSwapchainIndex;
    presentInfo.pResults = nullptr;

    VkResult r = vkQueuePresentKHR(queues->GetGraphics(), &presentInfo);

    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
    {
        Recreate(requestedExtent.width, requestedExtent.height, requestedVsync);
    }
}

bool Swapchain::Recreate(uint32_t newWidth, uint32_t newHeight, bool vsync)
{
    if (surfaceExtent.width == newWidth && surfaceExtent.height == newHeight && isVsync == vsync)
    {
        return false;
    }

    cmdManager->WaitDeviceIdle();

    VkSwapchainKHR old = DestroyWithoutSwapchain();
    Create(newWidth, newHeight, vsync, old);

    return true;
}

void Swapchain::Create(uint32_t newWidth, uint32_t newHeight, bool vsync, VkSwapchainKHR oldSwapchain)
{
    this->isVsync = vsync;

    assert(swapchain == VK_NULL_HANDLE);
    assert(swapchainImages.empty());
    assert(swapchainViews.empty());

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

    uint32_t imageCount = std::max(3U, surfCapabilities.minImageCount);
    if (surfCapabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, surfCapabilities.maxImageCount);
    }

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

        SET_DEBUG_NAME(device, swapchainImages[i], VK_OBJECT_TYPE_IMAGE, "Swapchain image");
        SET_DEBUG_NAME(device, swapchainViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, "Swapchain image view");
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

    CallCreateSubscribers();
}

void Swapchain::Destroy()
{
    VkSwapchainKHR old = DestroyWithoutSwapchain();
    vkDestroySwapchainKHR(device, old, nullptr);
}

VkSwapchainKHR RTGL1::Swapchain::DestroyWithoutSwapchain()
{
    vkDeviceWaitIdle(device);

    if (swapchain != VK_NULL_HANDLE)
    {
        CallDestroySubscribers();
    }

    for (VkImageView v : swapchainViews)
    {
        vkDestroyImageView(device, v, nullptr);
    }

    swapchainViews.clear();
    swapchainImages.clear();

    VkSwapchainKHR old = swapchain;
    swapchain = VK_NULL_HANDLE;

    return old;
}

void Swapchain::CallCreateSubscribers()
{
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            s->OnSwapchainCreate(this);
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

VkFormat Swapchain::GetSurfaceFormat() const
{
    return surfaceFormat.format;
}

uint32_t Swapchain::GetWidth() const
{
    return surfaceExtent.width;
}

uint32_t Swapchain::GetHeight() const
{
    return surfaceExtent.height;
}

uint32_t Swapchain::GetCurrentImageIndex() const
{
    return currentSwapchainIndex;
}

uint32_t Swapchain::GetImageCount() const
{
    assert(swapchainViews.size() == swapchainImages.size());
    return swapchainViews.size();
}

VkImageView Swapchain::GetImageView(uint32_t index) const
{
    assert(index < swapchainViews.size());
    return swapchainViews[index];
}

const VkImageView *Swapchain::GetImageViews() const
{
    if (!swapchainViews.empty())
    {
        return swapchainViews.data();
    }

    return nullptr;
}
