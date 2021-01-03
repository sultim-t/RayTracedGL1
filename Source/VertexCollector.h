#pragma once
#include <array>
#include <vector>

#include "Buffer.h"
#include "Common.h"
#include "RTGL1/RTGL1.h"

struct ShGeometryInstance;

// The class collects vertex data to buffers with shader struct types.
// Geometries are passed to the class by chunks and the result of collecting
// is a vertex buffer with ready data and infos for acceleration structure creation/building.
class VertexCollector
{
public:
    explicit VertexCollector(VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice, VkDeviceSize bufferSize, const VBProperties &properties);
    virtual ~VertexCollector();

    VertexCollector(const VertexCollector& other) = delete;
    VertexCollector(VertexCollector&& other) noexcept = delete;
    VertexCollector& operator=(const VertexCollector& other) = delete;
    VertexCollector& operator=(VertexCollector&& other) noexcept = delete;

    void BeginCollecting();
    uint32_t AddGeometry(const RgGeometryUploadInfo &info);
    void EndCollecting();

    const std::vector<uint32_t>
        &GetPrimitiveCounts() const;
    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetASGeometries() const;
    const std::vector<VkAccelerationStructureBuildRangeInfoKHR>
        &GetASBuildRangeInfos() const;

    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;
    VkBuffer GetGeometryInfosBuffer() const;

    // Clear data that was generated while collecting.
    // Should be called when blasGeometries is not needed anymore
    virtual void Reset();
    // Copy buffer from staging and set barrier
    // "isStatic" is required to determine what GLSL struct to use for copying
    void CopyFromStaging(VkCommandBuffer cmd, bool isStatic);

    // Update transform, mainly for movable static geometry as dynamic geometry
    // will be updated every frame and thus their transforms.
    void UpdateTransform(uint32_t geomIndex, const RgTransform &transform);

protected:
    virtual void PushPrimitiveCount(RgGeometryType type, uint32_t primCount);
    virtual void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom);
    virtual void PushRangeInfo(RgGeometryType type, const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo);

private:
    void CopyDataToStaging(const RgGeometryUploadInfo &info, uint32_t vertIndex, bool isStatic);

    bool CopyVertexDataFromStaging(VkCommandBuffer cmd, bool isStatic);
    bool CopyIndexDataFromStaging(VkCommandBuffer cmd);
    bool GetVertBufferCopyInfos(bool isStatic, std::array<VkBufferCopy, 5> &outInfos) const;

private:
    VkDevice device;
    VBProperties properties;

    Buffer stagingVertBuffer;
    Buffer vertBuffer;

    uint8_t *mappedVertexData;
    uint32_t *mappedIndexData;
    VkTransformMatrixKHR *mappedTransformData;

    ShGeometryInstance *mappedGeomInfosData;

    Buffer stagingIndexBuffer;
    Buffer indexBuffer;
    Buffer transforms;

    // buffer for getting info for geometry in BLAS
    Buffer geomInfosBuffer;

    uint32_t curVertexCount;
    uint32_t curIndexCount;
    uint32_t curPrimitiveCount;
    uint32_t curGeometryCount;

    std::vector<uint32_t> primitiveCounts;
    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfos;
};