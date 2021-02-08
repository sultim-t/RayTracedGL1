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

#include "Queues.h"

using namespace RTGL1;

uint32_t Queues::GetIndexGraphics() const
{
    return indexGraphics;
}

uint32_t Queues::GetIndexCompute() const
{
    return indexCompute;
}

uint32_t Queues::GetIndexTransfer() const
{
    return indexTransfer;
}

VkQueue Queues::GetGraphics() const
{
    return graphics;
}

VkQueue Queues::GetCompute() const
{
    return compute;
}

VkQueue Queues::GetTransfer() const
{
    return transfer;
}

Queues::Queues(VkPhysicalDevice physDevice, VkSurfaceKHR surface) :
    defaultQueuePriority(0),
    indexGraphics(UINT32_MAX),
    indexCompute(UINT32_MAX),
    indexTransfer(UINT32_MAX),
    graphics(VK_NULL_HANDLE),
    compute(VK_NULL_HANDLE),
    transfer(VK_NULL_HANDLE)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);

    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyProperties.size(); i++)
    {
        auto flags = queueFamilyProperties[i].queueFlags;

        VkBool32 presentSupported;
        VkResult r = vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface, &presentSupported);
        VK_CHECKERROR(r);

        if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            (flags & VK_QUEUE_COMPUTE_BIT)  != 0 &&
            (flags & VK_QUEUE_TRANSFER_BIT) != 0 &&
            presentSupported)
        {
            indexGraphics = i;
        }

        if ((flags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
            (flags & VK_QUEUE_COMPUTE_BIT)  != 0 &&
            (flags & VK_QUEUE_TRANSFER_BIT) == 0)
        {
            indexCompute = i;
        }

        if ((flags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
            (flags & VK_QUEUE_COMPUTE_BIT)  == 0 &&
            (flags & VK_QUEUE_TRANSFER_BIT) != 0)
        {
            indexTransfer = i;
        }
    }

    assert(indexGraphics != UINT32_MAX);

    if (indexCompute == UINT32_MAX)
    {
        indexCompute = indexGraphics;
    }

    if (indexTransfer == UINT32_MAX)
    {
        indexTransfer = indexGraphics;
    }
}

Queues::~Queues()
{}

void Queues::SetDevice(VkDevice device)
{
    vkGetDeviceQueue(device, indexGraphics, 0, &graphics);
    vkGetDeviceQueue(device, indexCompute, 0, &compute);
    vkGetDeviceQueue(device, indexTransfer, 0, &transfer);
}

void Queues::GetDeviceQueueCreateInfos(std::vector<VkDeviceQueueCreateInfo>& outInfos) const
{
    VkDeviceQueueCreateInfo queueInfo = {};

    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = indexGraphics;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &defaultQueuePriority;
    outInfos.push_back(queueInfo);

    if (indexCompute != indexGraphics)
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = indexCompute;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        outInfos.push_back(queueInfo);
    }

    if (indexTransfer != indexGraphics && indexTransfer != indexCompute)
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = indexTransfer;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        outInfos.push_back(queueInfo);
    }
}
