#pragma once
#include "Common.h"
#include "Buffer.h"
#include "RTGL1/RTGL1.h"

class VertexCollector
{
public:
    VertexCollector(std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties);
    virtual ~VertexCollector();

    void BeginCollecting();
    void AddGeometry(const RgGeometryCreateInfo &info);
    void EndCollecting();

    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetBlasGeometries() const;
    const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR>
        &GetBlasGeometriyTypes() const;

    // clear data that was generated while collecting,
    // should be called when blasGeometries is not needed anymore
    void Reset();

protected:
    virtual void PushGeometryType(RgGeometryType type, const VkAccelerationStructureCreateGeometryTypeInfoKHR &geomType);
    virtual void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom);

private:
    VkDevice device;
    VBProperties properties;

    std::weak_ptr<Buffer> vertBuffer;

    uint8_t *mappedData;
    // blasGeometries have pointers to these vectors
    std::vector<uint32_t> indices;
    std::vector<VkTransformMatrixKHR> transforms;
    uint32_t curVertexCount;
    uint32_t curIndexCount;
    uint32_t curGeometryCount;

    std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> blasGeometryTypes;
    std::vector<VkAccelerationStructureGeometryKHR> blasGeometries;
};