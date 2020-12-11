#include "VertexCollectorFiltered.h"
#include <utility>

VertexCollectorFiltered::VertexCollectorFiltered(
    std::shared_ptr<Buffer> stagingVertBuffer,
    std::shared_ptr<Buffer> vertBuffer,
    const VBProperties &properties, RgGeometryType filter)
    : VertexCollector(std::move(stagingVertBuffer), std::move(vertBuffer), properties)
{
    this->filter = filter;
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollectorFiltered::GetASGeometriesFiltered() const
{
    return geomsFiltered;
}

const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> &VertexCollectorFiltered::
GetASGeometryTypesFiltered() const
{
    return geomTypesFiltered;
}

const std::vector<VkAccelerationStructureBuildOffsetInfoKHR> &VertexCollectorFiltered::GetASBuildOffsetInfosFiltered() const
{
    return buildOffsetInfosFiltered;
}

void VertexCollectorFiltered::Reset()
{
    VertexCollector::Reset();

    geomTypesFiltered.clear();
    geomsFiltered.clear();
    buildOffsetInfosFiltered.clear();
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

void VertexCollectorFiltered::PushOffsetInfo(RgGeometryType type,
    const VkAccelerationStructureBuildOffsetInfoKHR& offsetInfo)
{
    if (type == filter)
    {
        buildOffsetInfosFiltered.push_back(offsetInfo);
    }
    else
    {
        VertexCollector::PushOffsetInfo(type, offsetInfo);
    }
}
