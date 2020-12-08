#pragma once

#include <vector>
#include "Common.h"

class Swapchain
{
public:

    // TODO: event OnSwapchainDestroy
    // TODO: event OnSwapchainCreate

private:
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D surfaceExtent;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
    uint32_t currentSwapchainIndex;
};