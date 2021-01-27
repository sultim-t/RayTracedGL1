// Copyright (c) 2020-2021 Sultim Tsyrendashiev
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "VertexCollectorFilter.h"
#include <utility>

VertexCollectorFilter::VertexCollectorFilter(
    VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice, 
    VkDeviceSize bufferSize, const VertexBufferProperties &properties, RgGeometryType filter)
    : VertexCollector(device, physDevice, bufferSize, properties)
{
    this->filter = filter;
}

VertexCollectorFilter::~VertexCollectorFilter()
{}

const std::vector<uint32_t> &VertexCollectorFilter::GetPrimitiveCountsFiltered() const
{
    return primCountFiltered;
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollectorFilter::GetASGeometriesFiltered() const
{
    return geomsFiltered;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollectorFilter::GetASBuildRangeInfosFiltered() const
{
    return buildRangeInfosFiltered;
}

void VertexCollectorFilter::Reset()
{
    VertexCollector::Reset();

    geomsFiltered.clear();
    primCountFiltered.clear();
    buildRangeInfosFiltered.clear();
}

void VertexCollectorFilter::PushPrimitiveCount(RgGeometryType type, uint32_t primCount)
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

void VertexCollectorFilter::PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom)
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

void VertexCollectorFilter::PushRangeInfo(RgGeometryType type,
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

uint32_t VertexCollectorFilter::GetGeometryCount() const
{
    return geomsFiltered.size() + VertexCollector::GetGeometryCount();
}
