#include "Buffer.h"

Buffer::Buffer()
    :
    device(VK_NULL_HANDLE),
    buffer(VK_NULL_HANDLE),
    memory(VK_NULL_HANDLE),
    address(0),
    size(0),
    isMapped(false)
{}

Buffer::~Buffer()
{
    assert(!isMapped);

    if (device == VK_NULL_HANDLE)
    {
        return;
    }

    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, memory, nullptr);
    }

    if (buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, buffer, nullptr);
    }
}

void Buffer::Init(VkDevice bdevice, const PhysicalDevice &physDevice, VkDeviceSize bsize, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties)
{
    device = bdevice;

    VkResult r;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bsize;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    r = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    VK_CHECKERROR(r);

    VkMemoryRequirements memReq = {};
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    memory = physDevice.AllocDeviceMemory(memReq, true);

    r = vkBindBufferMemory(device, buffer, memory, 0);
    VK_CHECKERROR(r);

    VkBufferDeviceAddressInfoKHR addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    address = vksGetBufferDeviceAddressKHR(device, &addrInfo);

    size = bsize;
}

void *Buffer::Map()
{
    assert(device != VK_NULL_HANDLE);
    assert(!isMapped);
    assert(memory != VK_NULL_HANDLE && size > 0);

    isMapped = true;
    void *mapped;

    VkResult r = vkMapMemory(device, memory, 0, size, 0, &mapped);
    VK_CHECKERROR(r);

    return mapped;
}

void Buffer::Unmap()
{
    assert(device != VK_NULL_HANDLE);
    assert(isMapped);
    isMapped = false;
    vkUnmapMemory(device, memory);
}

VkBuffer Buffer::GetBuffer() const
{
    return buffer;
}

VkDeviceMemory Buffer::GetMemory() const
{
    return memory;
}

VkDeviceAddress Buffer::GetAddress() const
{
    return address;
}

VkDeviceSize Buffer::GetSize() const
{
    return size;
}

bool Buffer::IsMapped() const
{
    return isMapped;
}
