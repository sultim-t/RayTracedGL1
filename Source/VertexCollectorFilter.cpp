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

VertexCollectorFilter::VertexCollectorFilter(VertexCollectorFilterTypeFlagBits _filter) : filter(_filter)
{}

VertexCollectorFilter::~VertexCollectorFilter()
{}

const std::vector<uint32_t> &VertexCollectorFilter::GetPrimitiveCounts() const
{
    return primitiveCounts;
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollectorFilter::GetASGeometries() const
{
    return asGeometries;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollectorFilter::GetASBuildRangeInfos() const
{
    return asBuildRangeInfos;
}

void VertexCollectorFilter::Reset()
{
    asGeometries.clear();
    primitiveCounts.clear();
    asBuildRangeInfos.clear();
}

void VertexCollectorFilter::PushPrimitiveCount(VertexCollectorFilterTypeFlags type, uint32_t primCount)
{
    if (type & (uint32_t)filter)
    {
        primitiveCounts.push_back(primCount);
    }
}

void VertexCollectorFilter::PushGeometry(VertexCollectorFilterTypeFlags type, const VkAccelerationStructureGeometryKHR &geom)
{
    if (type & (uint32_t)filter)
    {
        asGeometries.push_back(geom);
    }
}

void VertexCollectorFilter::PushRangeInfo(VertexCollectorFilterTypeFlags type, const VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
{
    if (type & (uint32_t)filter)
    {
        asBuildRangeInfos.push_back(rangeInfo);
    }
}

VertexCollectorFilterTypeFlagBits VertexCollectorFilter::GetFilter() const
{
    return filter;
}

uint32_t VertexCollectorFilter::GetGeometryCount() const
{
    return (uint32_t)asGeometries.size();
}
