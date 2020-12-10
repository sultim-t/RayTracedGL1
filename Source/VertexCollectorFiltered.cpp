#include "VertexCollectorFiltered.h"
#include <utility>

VertexCollectorFiltered::VertexCollectorFiltered(
    std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties, RgGeometryType filter)
    : VertexCollector(std::move(vertBuffer), properties)
{
    this->filter = filter;
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollectorFiltered::GetBlasGeometriesFiltered() const
{
    return geomsFiltered;
}

const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> &VertexCollectorFiltered::
GetBlasGeometriyTypesFiltered() const
{
    return geomTypesFiltered;
}

void VertexCollectorFiltered::PushGeometryType(RgGeometryType type,
                                               const VkAccelerationStructureCreateGeometryTypeInfoKHR &geomType)
{
    if (type == filter)
    {
        geomTypesFiltered.push_back(geomType);
    }
    else
    {
        VertexCollector::PushGeometryType(type, geomType);
    }
}

void VertexCollectorFiltered::PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom)
{
    if (type == filter)
    {
        geomsFiltered.push_back(geom);
    }
    else
    {
        VertexCollector::PushGeometry(type, geom);
    }
}
