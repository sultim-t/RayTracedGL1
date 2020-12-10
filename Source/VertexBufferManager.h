#pragma once
#include "VertexCollectorFiltered.h"

class VertexBufferManager
{
public:
public:
    VertexBufferManager(VkDevice device, const PhysicalDevice &physDevice, const RgInstanceCreateInfo &info);
    ~VertexBufferManager();

    void BeginStaticGeometry();
    void SubmitStaticGeometry();

    void BeginDynamicGeometry(uint32_t frameIndex);
    void SubmitDynamicGeometry();

    void AddGeometry(const RgGeometryCreateInfo &info);

private:
    void CreateDescriptors();

private:
    VkDevice device;
    uint32_t currentFrameIndex;

    std::shared_ptr<Buffer> staticVertsBuffer;
    std::shared_ptr<Buffer> staticVertsStaging;
    std::shared_ptr<Buffer> dynamicVertsBuffer[MAX_FRAMES_IN_FLIGHT];
    std::shared_ptr<Buffer> dynamicVertsStaging[MAX_FRAMES_IN_FLIGHT];

    std::shared_ptr<VertexCollectorFiltered> collectorStaticMovable;
    std::shared_ptr<VertexCollector> collectorDynamic[MAX_FRAMES_IN_FLIGHT];

    VkAccelerationStructureKHR staticBlas;
    VkAccelerationStructureKHR staticMovableBlas;
    VkAccelerationStructureKHR dynamicBlas[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];

    VBProperties properties;
};
