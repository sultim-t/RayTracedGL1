#pragma once

#include "Common.h"
#include "PhysicalDevice.h"

class Buffer
{
public:
    Buffer();
    ~Buffer();

    void Init(VkDevice device, const PhysicalDevice &physDevice,
                    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool getAddress = false);

    void* Map();
    void Unmap();


    VkBuffer GetBuffer() const;
    VkDeviceMemory GetMemory() const;
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
