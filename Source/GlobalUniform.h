#pragma once

#include "Buffer.h"

struct ShGlobalUniform;

class GlobalUniform
{
public:
    explicit GlobalUniform(VkDevice device, std::shared_ptr<PhysicalDevice> &physDevice, bool deviceLocal = false);
    ~GlobalUniform();

    GlobalUniform(const GlobalUniform &other) = delete;
    GlobalUniform(GlobalUniform &&other) noexcept = delete;
    GlobalUniform &operator=(const GlobalUniform &other) = delete;
    GlobalUniform &operator=(GlobalUniform &&other) noexcept = delete;

    // Send current data
    void Upload(uint32_t frameIndex);

    // Getters for modifying uniform buffer data that will be uploaded
    ShGlobalUniform *GetData();
    const ShGlobalUniform *GetData() const;

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

private:
    void CreateDescriptors();
    void SetData(uint32_t frameIndex, const void *data, VkDeviceSize dataSize);

private:
    VkDevice device;

    std::shared_ptr<ShGlobalUniform> uniformData;
    Buffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool        descPool;
    VkDescriptorSetLayout   descSetLayout;
    VkDescriptorSet         descSets[MAX_FRAMES_IN_FLIGHT];
};