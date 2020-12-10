#pragma once

#include "VertexCollector.h"

class VertexCollectorFiltered : public VertexCollector
{
public:
    VertexCollectorFiltered(
        std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties,
        RgGeometryType filter);

    const std::vector<VkAccelerationStructureGeometryKHR>
        &GetBlasGeometriesFiltered() const;
    const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR>
        &GetBlasGeometriyTypesFiltered() const;

protected:
    void PushGeometryType(RgGeometryType type, const VkAccelerationStructureCreateGeometryTypeInfoKHR& geomType) override;
    void PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR& geom) override;

private:
    RgGeometryType filter;

    std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> geomTypesFiltered;
    std::vector<VkAccelerationStructureGeometryKHR> geomsFiltered;
};