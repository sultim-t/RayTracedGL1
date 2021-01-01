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

void Utils::ASBuildMemoryBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask =
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = 
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    // wait for all building
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

void Utils::WaitForFence(VkDevice device, VkFence fence)
{
    VkResult r = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    VK_CHECKERROR(r);
}

void Utils::ResetFence(VkDevice device, VkFence fence)
{
    VkResult r = vkResetFences(device, 1, &fence);
    VK_CHECKERROR(r);
}

void Utils::WaitAndResetFence(VkDevice device, VkFence fence)
{
    VkResult r;

    r = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    VK_CHECKERROR(r);

    r = vkResetFences(device, 1, &fence);
    VK_CHECKERROR(r);
}

uint32_t Utils::Align(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
