#pragma once

#include <vector>
#include "Common.h"

class Queues
{
public:
    explicit Queues(VkPhysicalDevice physDevice);
    void InitQueues(VkDevice device);

    void GetDeviceQueueCreateInfos(std::vector<VkDeviceQueueCreateInfo> &outInfos) const;
    uint32_t GetQueueFamilyIndex(VkQueueFlagBits queueFlags) const;

    uint32_t GetIndexGraphics() const;
    uint32_t GetIndexCompute() const;
    uint32_t GetIndexTransfer() const;
    VkQueue GetGraphics() const;
    VkQueue GetCompute() const;
    VkQueue GetTransfer() const;

private:
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;

    uint32_t indexGraphics;
    uint32_t indexCompute;
    uint32_t indexTransfer;

    VkQueue graphics;
    VkQueue compute;
    VkQueue transfer;
};