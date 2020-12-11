#pragma once

#include <vector>
#include "Common.h"

class PhysicalDevice
{
public:
    explicit PhysicalDevice(VkInstance instance, uint32_t selectedPhysDevice);
    void SetDevice(VkDevice device);

    VkPhysicalDevice Get() const;
    uint32_t GetMemoryTypeIndex(uint32_t memoryTypeBits, VkFlags requirementsMask) const;
    VkPhysicalDeviceMemoryProperties GetMemoryProperties() const;
    VkPhysicalDeviceRayTracingPropertiesKHR GetRayTracingProperties() const;

    // if addressQuery=true address can be queried
    VkDeviceMemory AllocDeviceMemory(const VkMemoryRequirements &memReqs, bool addressQuery = false) const;
    VkDeviceMemory AllocDeviceMemory(const VkMemoryRequirements2 &memReqs2, bool addressQuery = false) const;

    void FreeDeviceMemory(VkDeviceMemory memory) const;

private:
    VkDevice device;
    // selected physical device
    VkPhysicalDevice physDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties;
};