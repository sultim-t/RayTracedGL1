#include "VertexCollectorFiltered.h"
#include <utility>

VertexCollectorFiltered::VertexCollectorFiltered(
    VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice, 
    VkDeviceSize bufferSize, const VertexBufferProperties &properties, RgGeometryType filter)
    : VertexCollector(device, physDevice, bufferSize, properties)
{
    this->filter = filter;
}

VertexCollectorFiltered::~VertexCollectorFiltered()
{}

const std::vector<uint32_t> &VertexCollectorFiltered::GetPrimitiveCountsFiltered() const
{
    return primCountFiltered;
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollectorFiltered::GetASGeometriesFiltered() const
{
    return geomsFiltered;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollectorFiltered::GetASBuildRangeInfosFiltered() const
{
    return buildRangeInfosFiltered;
}

void VertexCollectorFiltered::Reset()
{
    VertexCollector::Reset();

    geomsFiltered.clear();
    primCountFiltered.clear();
    buildRangeInfosFiltered.clear();
}

void VertexCollectorFiltered::PushPrimitiveCount(RgGeometryType type, uint32_t primCount)
{
    if (type == filter)
    {
        primCountFiltered.push_back(primCount);
    }
    else
    {
        VertexCollector::PushPrimitiveCount(type, primCount);
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

void VertexCollectorFiltered::PushRangeInfo(RgGeometryType type,
    const VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
{
    if (type == filter)
    {
        buildRangeInfosFiltered.push_back(rangeInfo);
    }
    else
    {
        VertexCollector::PushRangeInfo(type, rangeInfo);
    }
}

uint32_t VertexCollectorFiltered::GetGeometryCount() const
{
    return geomsFiltered.size() + VertexCollector::GetGeometryCount();
}
