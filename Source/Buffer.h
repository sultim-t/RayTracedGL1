#pragma once

#include "Common.h"
#include "PhysicalDevice.h"

class Buffer
{
public:
    Buffer();
    ~Buffer();

    // Create VkBuffer, allocate memory and bind it
    void Init(VkDevice device, const PhysicalDevice &physDevice,
              VkDeviceSize size, VkBufferUsageFlags usage, 
              VkMemoryPropertyFlags properties);
    void Destroy();

    void* Map();
    void Unmap();


    VkBuffer GetBuffer() const;
    VkDeviceMemory GetMemory() const;
    // To get address usage flags must contain VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    VkDeviceAddress GetAddress() const;
    VkDeviceSize GetSize() const;
    bool IsMapped() const;

protected:
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    VkDeviceSize size;

private:
    bool isMapped;
};
