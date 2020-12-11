#pragma once
#include "ASBuilder.h"
#include "VertexCollectorFiltered.h"
#include "CommandBufferManager.h"
#include "ScratchBuffer.h"

class VertexBufferManager
{
public:
    VertexBufferManager(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice,
                        std::shared_ptr<CommandBufferManager> cmdManager, 
                        const RgInstanceCreateInfo &info);
    ~VertexBufferManager();

    void BeginStaticGeometry();
    void AddStaticGeometry(const RgGeometryCreateInfo &info);
    // Submitting static geometry to the building is a heavy operation
    // with waiting for it to complete.
    void SubmitStaticGeometry();

    void BeginDynamicGeometry(uint32_t frameIndex);
    void AddDynamicGeometry(const RgGeometryCreateInfo &info);
    void SubmitDynamicGeometry(VkCommandBuffer cmd);

    // Update transform for static movable geometry
    void UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform);
    // After updating transforms, acceleration structures should be rebuilt
    void ResubmitStaticMovable(VkCommandBuffer cmd);

    VkAccelerationStructureKHR GetStaticBLAS() const { return staticBlas.as; }
    VkAccelerationStructureKHR GetStaticMovableBLAS() const { return staticMovableBlas.as; }
    VkAccelerationStructureKHR GetDynamicBLAS(uint32_t frameIndex) const { return dynamicBlas[frameIndex].as; }

private:
    struct AccelerationStructure
    {
        VkAccelerationStructureKHR as;
        VkDeviceMemory memory;
    };

private:
    void CreateDescriptors();

    uint32_t AddGeometry(const RgGeometryCreateInfo &info);

    void AllocBindASMemory(AccelerationStructure &as);
    void DestroyAS(AccelerationStructure &as);

private:
    VkDevice device;
    std::shared_ptr<PhysicalDevice> physDevice;

    uint32_t currentFrameIndex;

    // buffers for static, movable static geometry
    std::shared_ptr<Buffer> staticVertsBuffer;
    std::shared_ptr<Buffer> staticVertsStaging;
    VkFence staticCopyFence;

    // buffers for dynamic geometry
    std::shared_ptr<Buffer> dynamicVertsBuffer[MAX_FRAMES_IN_FLIGHT];
    std::shared_ptr<Buffer> dynamicVertsStaging[MAX_FRAMES_IN_FLIGHT];

    // for filling buffers
    std::shared_ptr<VertexCollectorFiltered> collectorStaticMovable;
    std::shared_ptr<VertexCollector> collectorDynamic[MAX_FRAMES_IN_FLIGHT];

    // building
    std::shared_ptr<ScratchBuffer> scratchBuffer;
    std::shared_ptr<ASBuilder> asBuilder;

    std::shared_ptr<CommandBufferManager> cmdManager;

    AccelerationStructure staticBlas;
    AccelerationStructure staticMovableBlas;
    AccelerationStructure dynamicBlas[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];

    VBProperties properties;
};
