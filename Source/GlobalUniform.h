#pragma once

#include "Buffer.h"

struct ShGlobalUniform;

class GlobalUniform
{
public:
    explicit GlobalUniform(VkDevice device, const PhysicalDevice &physDevice, bool deviceLocal = false);
    ~GlobalUniform();

    GlobalUniform(const GlobalUniform &other) = delete;
    GlobalUniform(GlobalUniform &&other) noexcept = delete;
    GlobalUniform &operator=(const GlobalUniform &other) = delete;
    GlobalUniform &operator=(GlobalUniform &&other) noexcept = delete;

    void Upload(uint32_t frameIndex);

    ShGlobalUniform *GetData();
    const ShGlobalUniform *GetData() const;
    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;

private:
    void CreateDescriptors();
    void SetData(uint32_t frameIndex, const void *data, VkDeviceSize dataSize);

private:
    VkDevice device;

    std::shared_ptr<ShGlobalUniform> uniformData;
    Buffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout   descSetLayout;
    VkDescriptorPool        descPool;
    VkDescriptorSet         descSets[MAX_FRAMES_IN_FLIGHT];
};