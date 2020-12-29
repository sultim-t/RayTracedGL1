#pragma once

#include "Common.h"

class Utils
{
public:
    static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        const VkImageSubresourceRange &subresourceRange);

    static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        const VkImageSubresourceRange &subresourceRange);

    static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

    static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout);

    static void WaitForFence(VkDevice device, VkFence fence);
    static void ResetFence(VkDevice device, VkFence fence);
    static void WaitAndResetFence(VkDevice device, VkFence fence);
};