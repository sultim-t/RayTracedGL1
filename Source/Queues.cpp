#include "Queues.h"

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

Queues::Queues(VkPhysicalDevice physDevice, VkSurfaceKHR surface)
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

    graphics = VK_NULL_HANDLE;
    compute = VK_NULL_HANDLE;
    transfer = VK_NULL_HANDLE;
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
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo queueInfo = {};
    const float defaultQueuePriority = 0;

    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = indexGraphics;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &defaultQueuePriority;
    queueCreateInfos.push_back(queueInfo);

    if (indexCompute != indexGraphics)
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = indexCompute;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    if (indexTransfer != indexGraphics && indexTransfer != indexCompute)
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = indexTransfer;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    }
}
