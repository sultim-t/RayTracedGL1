#pragma once

#include "Common.h"
#include "CommandBufferManager.h"
#include "PhysicalDevice.h"
#include "ISwapchainDependency.h"

// Temporary class for simple storing of a ray traced image 
class BasicStorageImage final : public ISwapchainDependency
{
public:
    explicit BasicStorageImage(VkDevice device,
                      std::shared_ptr<PhysicalDevice> physDevice,
                      std::shared_ptr<CommandBufferManager> cmdManager);
    ~BasicStorageImage() override;

    BasicStorageImage(const BasicStorageImage &other) = delete;
    BasicStorageImage(BasicStorageImage &&other) noexcept = delete;
    BasicStorageImage &operator=(const BasicStorageImage &other) = delete;
    BasicStorageImage &operator=(BasicStorageImage &&other) noexcept = delete;

    void Barrier(VkCommandBuffer cmd);

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const
    {
        return descSets[frameIndex];
    }

    VkDescriptorSetLayout GetDescSetLayout() const
    {
        return descLayout;
    }

    void OnSwapchainCreate(uint32_t newWidth, uint32_t newHeight) override;
    void OnSwapchainDestroy() override;

private:
    void CreateImage(uint32_t width, uint32_t height);
    void DestroyImage();

    void CreateDescriptors();
    void UpdateDescriptors();

public:
    VkImage image;
    VkImageLayout imageLayout;
    uint32_t width, height;

private:
    VkDevice device;
    std::shared_ptr<PhysicalDevice> physDevice;
    std::shared_ptr<CommandBufferManager> cmdManager;

    VkImageView view;
    VkDeviceMemory memory;

    VkDescriptorSetLayout descLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];
};

