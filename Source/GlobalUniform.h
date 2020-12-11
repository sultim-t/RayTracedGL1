#pragma once

#include "Buffer.h"

class GlobalUniform
{
public:
    explicit GlobalUniform(VkDevice device, const PhysicalDevice &physDevice, bool deviceLocal = false);
    ~GlobalUniform();

    void SetData(uint32_t frameIndex, const void *data, VkDeviceSize dataSize);

private:
    VkDevice device;
    Buffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout       descSetLayout;
    VkDescriptorPool            descPool;
    VkDescriptorSet             descSets[MAX_FRAMES_IN_FLIGHT];
};