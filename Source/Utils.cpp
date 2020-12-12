#include "Utils.h"

void Utils::BarrierImage(
    VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask, const VkImageSubresourceRange &subresourceRange)
{
    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.image = image;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(
        cmd,
        srcStageMask, dstStageMask, 0,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier);
}

void Utils::BarrierImage(
    VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange &subresourceRange)
{
    BarrierImage(
        cmd, image, srcAccessMask, dstAccessMask, oldLayout, newLayout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
}

void Utils::BarrierImage(
    VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    BarrierImage(
        cmd, image, srcAccessMask, dstAccessMask, oldLayout, newLayout,
        srcStageMask, dstStageMask, subresourceRange);
}

void Utils::BarrierImage(
    VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    BarrierImage(
        cmd, image, srcAccessMask, dstAccessMask, oldLayout, newLayout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
}
