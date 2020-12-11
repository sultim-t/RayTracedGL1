#pragma once

#include "VertexCollector.h"

// This class collects vertex data in the same way as VertexCollector
// but geometries with type==filter will be stored to the separate arrays
class VertexCollectorFiltered : public VertexCollector
{
public:
    explicit VertexCollectorFiltered(
        std::shared_ptr<Buffer> stagingVertBuffer, std::shared_ptr<Buffer> vertBuffer, 
        const VBProperties &properties, RgGeometryType filter);

    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetASGeometriesFiltered() const;
    const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR>
        &GetASGeometryTypesFiltered() const;
    const std::vector<VkAccelerationStructureBuildOffsetInfoKHR>
        &GetASBuildOffsetInfosFiltered() const;

    void Reset() override;

protected:
    void PushGeometryType(RgGeometryType type, const VkAccelerationStructureCreateGeometryTypeInfoKHR& geomType) override;
    void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR& geom) override;
    void PushOffsetInfo(RgGeometryType type, const VkAccelerationStructureBuildOffsetInfoKHR &offsetInfo) override;

private:
    RgGeometryType filter;

    std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> geomTypesFiltered;
    std::vector<VkAccelerationStructureGeometryKHR> geomsFiltered;
    std::vector<VkAccelerationStructureBuildOffsetInfoKHR> buildOffsetInfosFiltered;
};