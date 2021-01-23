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

bool Utils::AreViewportsSame(const VkViewport &a, const VkViewport &b)
{
    // special epsilons for viewports
    const float eps = 0.1f;
    const float depthEps = 0.001f;

    return
        std::abs(a.x        - b.x)          < eps &&
        std::abs(a.y        - b.y)          < eps &&
        std::abs(a.width    - b.width)      < eps &&
        std::abs(a.height   - b.height)     < eps &&
        std::abs(a.minDepth - b.minDepth)   < depthEps &&
        std::abs(a.maxDepth - b.maxDepth)   < depthEps;
}

bool Utils::IsDefaultViewport(const RgViewport &viewport)
{
    constexpr float eps = 0.01f;

    return
        std::abs(viewport.width)    < eps &&
        std::abs(viewport.height)   < eps &&
        std::abs(viewport.x)        < eps &&
        std::abs(viewport.y)        < eps;
}

