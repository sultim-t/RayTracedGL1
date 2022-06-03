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

#include <cmath>

using namespace RTGL1;

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

void RTGL1::Utils::WaitAndResetFences(VkDevice device, VkFence fence_A, VkFence fence_B)
{
    VkResult r;
    VkFence fences[2];
    uint32_t count = 0;

    if (fence_A != VK_NULL_HANDLE)
    {
        fences[count++] = fence_A;
    }

    if (fence_B != VK_NULL_HANDLE)
    {
        fences[count++] = fence_B;
    }

    r = vkWaitForFences(device, count, fences, VK_TRUE, UINT64_MAX);
    VK_CHECKERROR(r);

    r = vkResetFences(device, count, fences);
    VK_CHECKERROR(r);
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

constexpr float ALMOST_ZERO_THRESHOLD = 0.01f;

bool RTGL1::Utils::IsAlmostZero(const RgFloat3D &v)
{
    return
        std::abs(v.data[0]) +
        std::abs(v.data[1]) +
        std::abs(v.data[2]) < ALMOST_ZERO_THRESHOLD;
}

bool RTGL1::Utils::IsAlmostZero(const RgMatrix3D &m)
{
    float s = 0;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            s += std::abs(m.matrix[i][j]);
        }
    }

    return s < ALMOST_ZERO_THRESHOLD;
}

float RTGL1::Utils::Dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float RTGL1::Utils::Length(const float v[3])
{
    return sqrtf(Dot(v, v));
}


void RTGL1::Utils::Normalize(float inout[3])
{
    float len = Length(inout);

    if (len > 0.01f)
    {
        inout[0] /= len;
        inout[1] /= len;
        inout[2] /= len;
    }
    else
    {
        assert(0);
        inout[0] = inout[1] = inout[2] = 0.0f;
    }
}


void RTGL1::Utils::Cross(const float a[3], const float b[3], float r[3])
{
    r[0] = a[1] * b[2] - a[2] * b[1];
    r[1] = a[2] * b[0] - a[0] * b[2];
    r[2] = a[0] * b[1] - a[1] * b[0];
}

RgFloat3D RTGL1::Utils::GetUnnormalizedNormal(const RgFloat3D positions[3])
{
    const float *a = positions[0].data;
    const float *b = positions[1].data;
    const float *c = positions[2].data;

    float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    float e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };

    RgFloat3D n = {};
    Cross(e1, e2, n.data);

    return n;
}

bool RTGL1::Utils::GetNormalAndArea(const RgFloat3D positions[3], RgFloat3D &normal, float &area)
{
    normal = GetUnnormalizedNormal(positions);

    float len = Length(normal.data);
    normal.data[0] /= len;
    normal.data[1] /= len;
    normal.data[2] /= len;

    area = len * 0.5f;
    return area > 0.01f;
}


void RTGL1::Utils::SetMatrix3ToGLSLMat4(float dst[16], const RgMatrix3D &src)
{
    const bool toColumnMajor = true;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            float v;

            if (i < 3 && j < 3)
            {
                v = toColumnMajor ? src.matrix[j][i] : src.matrix[i][j];
            }
            else
            {
                v = i == j ? 1 : 0;
            }

            dst[i * 4 + j] = v;
        }
    }
}

uint32_t RTGL1::Utils::GetPreviousByModulo(uint32_t value, uint32_t count)
{
    assert(count > 0);
    return (value + (count - 1)) % count;
}

uint32_t RTGL1::Utils::GetWorkGroupCount(float size, uint32_t groupSize)
{
    return GetWorkGroupCount((uint32_t)std::ceil(size), groupSize);
}

uint32_t RTGL1::Utils::GetWorkGroupCount(uint32_t size, uint32_t groupSize)
{
    if (groupSize == 0)
    {
        assert(0);
        return 0;
    }

    return 1 + (size + (groupSize - 1)) / groupSize;
}
