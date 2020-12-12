#pragma once

#include "Common.h"

class Utils
{
public:
    inline static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        const VkImageSubresourceRange &subresourceRange);

    inline static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        const VkImageSubresourceRange &subresourceRange);

    inline static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

    inline static void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout);

};