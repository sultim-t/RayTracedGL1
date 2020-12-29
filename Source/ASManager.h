#pragma once

#include "ASBuilder.h"
#include "VertexCollectorFiltered.h"
#include "CommandBufferManager.h"
#include "ScratchBuffer.h"

class ASManager
{
public:
    ASManager(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice,
              std::shared_ptr<CommandBufferManager> cmdManager,
              const RgInstanceCreateInfo &info);
    ~ASManager();

    ASManager(const ASManager& other) = delete;
    ASManager(ASManager&& other) noexcept = delete;
    ASManager& operator=(const ASManager& other) = delete;
    ASManager& operator=(ASManager&& other) noexcept = delete;

    // Static geometry recording is frameIndex-agnostic
    void BeginStaticGeometry();
    uint32_t AddStaticGeometry(const RgGeometryUploadInfo &info);
    // Submitting static geometry to the building is a heavy operation
    // with waiting for it to complete.
    void SubmitStaticGeometry();
    // If all the added geometries must be removed, call this function before submitting
    void ResetStaticGeometry();

    void BeginDynamicGeometry(uint32_t frameIndex);
    uint32_t AddDynamicGeometry(const RgGeometryUploadInfo &info, uint32_t frameIndex);
    void SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex);

    // Update transform for static movable geometry
    void UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform);
    // After updating transforms, acceleration structures should be rebuilt
    void ResubmitStaticMovable(VkCommandBuffer cmd);

    void BuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex);
    VkDescriptorSet GetBuffersDescSet(uint32_t frameIndex) const;
    VkDescriptorSet GetTLASDescSet(uint32_t frameIndex) const;

    VkDescriptorSetLayout GetBuffersDescSetLayout() const;
    VkDescriptorSetLayout GetTLASDescSetLayout() const;

private:
    struct AccelerationStructure
    {
        VkAccelerationStructureKHR as = VK_NULL_HANDLE;
        Buffer buffer = {};
    };

private:
    void CreateDescriptors();
    void UpdateBufferDescriptors();
    void UpdateASDescriptors(uint32_t frameIndex);

    void CreateASBuffer(AccelerationStructure &as, VkDeviceSize size);
    void DestroyAS(AccelerationStructure &as);
    VkDeviceAddress GetASAddress(const AccelerationStructure &as);
    VkDeviceAddress GetASAddress(VkAccelerationStructureKHR as);

private:
    VkDevice device;
    std::shared_ptr<PhysicalDevice> physDevice;

    // for adding dynamic geometry
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

    // top level AS
    Buffer instanceBuffers[MAX_FRAMES_IN_FLIGHT];
    AccelerationStructure tlas[MAX_FRAMES_IN_FLIGHT];

    // TLAS and buffer descriptors
    VkDescriptorPool descPool;

    VkDescriptorSetLayout buffersDescSetLayout;
    VkDescriptorSet buffersDescSets[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout asDescSetLayout;
    VkDescriptorSet asDescSets[MAX_FRAMES_IN_FLIGHT];

    VBProperties properties;
};
