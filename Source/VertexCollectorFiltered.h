#pragma once

#include "VertexCollector.h"

// This class collects vertex data in the same way as VertexCollector
// but geometries with type==filter will be stored to the separate arrays
class VertexCollectorFiltered : public VertexCollector
{
public:
    explicit VertexCollectorFiltered(
        VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice,
        VkDeviceSize bufferSize, const VertexBufferProperties &properties, RgGeometryType filter);
    ~VertexCollectorFiltered() override;

    VertexCollectorFiltered(const VertexCollectorFiltered& other) = delete;
    VertexCollectorFiltered(VertexCollectorFiltered&& other) noexcept = delete;
    VertexCollectorFiltered& operator=(const VertexCollectorFiltered& other) = delete;
    VertexCollectorFiltered& operator=(VertexCollectorFiltered&& other) noexcept = delete;

    const std::vector<uint32_t>
        &GetPrimitiveCountsFiltered() const;
    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetASGeometriesFiltered() const;
    const std::vector<VkAccelerationStructureBuildRangeInfoKHR>
        &GetASBuildRangeInfosFiltered() const;

    void Reset() override;

protected:
    void PushPrimitiveCount(RgGeometryType type, uint32_t primCount) override;
    void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR& geom) override;
    void PushRangeInfo(RgGeometryType type, const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo) override;

private:
    RgGeometryType filter;

    std::vector<uint32_t> primCountFiltered;
    std::vector<VkAccelerationStructureGeometryKHR> geomsFiltered;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfosFiltered;
};