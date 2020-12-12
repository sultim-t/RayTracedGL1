#pragma once

#include "Common.h"
#include "ShaderManager.h"
#include "Buffer.h"

class RayTracingPipeline
{
public:
    explicit RayTracingPipeline(VkDevice device, const PhysicalDevice &physDevice, const ShaderManager &shaderManager);
    ~RayTracingPipeline();

    RayTracingPipeline(const RayTracingPipeline& other) = delete;
    RayTracingPipeline(RayTracingPipeline&& other) noexcept = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline& other) = delete;
    RayTracingPipeline& operator=(RayTracingPipeline&& other) noexcept = delete;

    void GetEntries(VkStridedBufferRegionKHR &raygenEntry,
                    VkStridedBufferRegionKHR &missEntry,
                    VkStridedBufferRegionKHR &hitEntry,
                    VkStridedBufferRegionKHR &callableEntry);

private:
    void CreateDescriptors();
    void CreateSBT(const PhysicalDevice &physDevice);

    void AddGeneralGroup(uint32_t generalIndex);
    void AddHitGroup(uint32_t closestHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex, uint32_t intersectionIndex);

private:
    VkDevice device;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    VkPipelineLayout rtPipelineLayout;
    VkPipeline rtPipeline;

    VkDescriptorPool rtDescPool;
    VkDescriptorSetLayout rtDescSetLayout;
    VkDescriptorSet rtDescSets[MAX_FRAMES_IN_FLIGHT];

    std::shared_ptr<Buffer> shaderBindingTable;
    VkDeviceSize sbtAlignment;
    VkDeviceSize sbtHandleSize;
    VkDeviceSize sbtSize;
};