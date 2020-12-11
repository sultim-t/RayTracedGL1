#pragma once
#include "Common.h"
#include "Buffer.h"
#include "RTGL1/RTGL1.h"

// The class collects vertex data to buffers with shader struct types.
// Geometries are passed to the class by chunks and the result of collecting
// is a vertex buffer with ready data and infos for acceleration structure creation/building.
class VertexCollector
{
public:
    VertexCollector(std::shared_ptr<Buffer> stagingVertBuffer, std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties);
    virtual ~VertexCollector();

    void BeginCollecting();
    uint32_t AddGeometry(const RgGeometryCreateInfo &info);
    void EndCollecting();

    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetASGeometries() const;
    const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR>
        &GetASGeometryTypes() const;
    const std::vector<VkAccelerationStructureBuildOffsetInfoKHR>
        &GetASBuildOffsetInfos() const;

    // Clear data that was generated while collecting.
    // Should be called when blasGeometries is not needed anymore
    virtual void Reset();
    // Copy buffer from staging and set barrier
    void CopyFromStaging(VkCommandBuffer cmd);

    // Update transform, mainly for movable static geometry as dynamic geometry
    // will be updated every frame and thus their transforms.
    void UpdateTransform(uint32_t geomIndex, const RgTransform &transform);

protected:
    virtual void PushGeometryType(RgGeometryType type, const VkAccelerationStructureCreateGeometryTypeInfoKHR &geomType);
    virtual void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom);
    virtual void PushOffsetInfo(RgGeometryType type, const VkAccelerationStructureBuildOffsetInfoKHR &offsetInfo);

private:
    void CopyDataToStaging(const RgGeometryCreateInfo &info, uint32_t vertIndex, bool isStatic);
    void GetCopyInfos(bool isStatic, std::vector<VkBufferCopy> &outInfos) const;

private:
    VkDevice device;
    VBProperties properties;

    std::weak_ptr<Buffer> stagingVertBuffer;
    std::weak_ptr<Buffer> vertBuffer;

    uint8_t *mappedData;
    // blasGeometries have pointers to these vectors
    std::vector<uint32_t> indices;
    std::vector<VkTransformMatrixKHR> transforms;
    // used to determine Rg type of added geometry
    std::vector<RgGeometryType> rgTypes;

    uint32_t curVertexCount;
    uint32_t curIndexCount;
    uint32_t curGeometryCount;

    std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> asGeometryTypes;
    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildOffsetInfoKHR> asBuildOffsetInfos;
};