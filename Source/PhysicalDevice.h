#pragma once

#include "Common.h"

class PhysicalDevice
{
public:
    explicit PhysicalDevice(VkInstance instance, uint32_t selectedPhysDevice);
    ~PhysicalDevice() = default;

    PhysicalDevice(const PhysicalDevice& other) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept = delete;
    PhysicalDevice& operator=(const PhysicalDevice& other) = delete;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = delete;

    void SetDevice(VkDevice device);

    VkPhysicalDevice Get() const;
    uint32_t GetMemoryTypeIndex(uint32_t memoryTypeBits, VkFlags requirementsMask) const;
    const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const;
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &GetRTPipelineProperties() const;

    // if addressQuery=true address can be queried
    VkDeviceMemory AllocDeviceMemory(const VkMemoryRequirements &memReqs, VkMemoryPropertyFlags properties, bool addressQuery = false) const;
    VkDeviceMemory AllocDeviceMemory(const VkMemoryRequirements2 &memReqs2, VkMemoryPropertyFlags properties, bool addressQuery = false) const;

    void FreeDeviceMemory(VkDeviceMemory memory) const;

private:
    VkDevice device;
    // selected physical device
    VkPhysicalDevice physDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties;
};