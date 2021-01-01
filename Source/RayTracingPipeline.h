#pragma once

#include "ASManager.h"
#include "GlobalUniform.h"
#include "ShaderManager.h"

class RayTracingPipeline
{
public:
    // TODO: remove imagesSetLayout
    explicit RayTracingPipeline(
        VkDevice device,
        const std::shared_ptr<PhysicalDevice> &physDevice,
        const std::shared_ptr<ShaderManager> &shaderManager,
        const std::shared_ptr<ASManager> &asManager,
        const std::shared_ptr<GlobalUniform> &uniform,
        VkDescriptorSetLayout imagesSetLayout
    );
    ~RayTracingPipeline();

    RayTracingPipeline(const RayTracingPipeline& other) = delete;
    RayTracingPipeline(RayTracingPipeline&& other) noexcept = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline& other) = delete;
    RayTracingPipeline& operator=(RayTracingPipeline&& other) noexcept = delete;

    void Bind(VkCommandBuffer cmd);

    void GetEntries(VkStridedDeviceAddressRegionKHR &raygenEntry,
                    VkStridedDeviceAddressRegionKHR &missEntry,
                    VkStridedDeviceAddressRegionKHR &hitEntry,
                    VkStridedDeviceAddressRegionKHR &callableEntry) const;
    VkPipelineLayout GetLayout() const;

private:
    void CreateSBT(const std::shared_ptr<PhysicalDevice> &physDevice);

    void AddGeneralGroup(uint32_t generalIndex);
    void AddHitGroup(uint32_t closestHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex, uint32_t intersectionIndex);

private:
    VkDevice device;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    VkPipelineLayout rtPipelineLayout;
    VkPipeline rtPipeline;

    std::shared_ptr<Buffer> shaderBindingTable;

    uint32_t groupBaseAlignment;
    uint32_t handleSize;
    uint32_t alignedHandleSize;
};
