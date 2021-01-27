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

#pragma once

#include "VertexCollector.h"

// This class collects vertex data in the same way as VertexCollector
// but geometries with type==filter will be stored to the separate arrays
class VertexCollectorFilter : public VertexCollector
{
public:
    explicit VertexCollectorFilter(
        VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice,
        VkDeviceSize bufferSize, const VertexBufferProperties &properties, RgGeometryType filter);
    ~VertexCollectorFilter() override;

    VertexCollectorFilter(const VertexCollectorFilter& other) = delete;
    VertexCollectorFilter(VertexCollectorFilter&& other) noexcept = delete;
    VertexCollectorFilter& operator=(const VertexCollectorFilter& other) = delete;
    VertexCollectorFilter& operator=(VertexCollectorFilter&& other) noexcept = delete;

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

    uint32_t GetGeometryCount() const override;

private:
    RgGeometryType filter;

    std::vector<uint32_t> primCountFiltered;
    std::vector<VkAccelerationStructureGeometryKHR> geomsFiltered;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfosFiltered;
};