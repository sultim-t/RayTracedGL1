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

#include <vector>

#include "Common.h"
#include "ScratchBuffer.h"

namespace RTGL1
{

class ASBuilder
{
public:
    explicit ASBuilder(VkDevice device, std::shared_ptr<ScratchBuffer> commonScratchBuffer);


    ASBuilder(const ASBuilder& other) = delete;
    ASBuilder(ASBuilder&& other) noexcept = delete;
    ASBuilder& operator=(const ASBuilder& other) = delete;
    ASBuilder& operator=(ASBuilder&& other) noexcept = delete;


    // pGeometries is a pointer to an array of size "geometryCount",
    // pRangeInfos is an array of size "geometryCount".
    // All pointers must be valid until BuildBottomLevel is called
    void AddBLAS(
        VkAccelerationStructureKHR as, uint32_t geometryCount,
        const VkAccelerationStructureGeometryKHR *pGeometries,
        const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfos,
        const VkAccelerationStructureBuildSizesInfoKHR &buildSizes,
        bool fastTrace, bool update);

    void BuildBottomLevel(VkCommandBuffer cmd);


    // pGeometry is a pointer to one AS geometry,
    // pRangeInfo is a pointer to build range info.
    // All pointers must be valid until BuildTopLevel is called
    void AddTLAS(
        VkAccelerationStructureKHR as,
        const VkAccelerationStructureGeometryKHR *pGeometry,
        const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfo,
        const VkAccelerationStructureBuildSizesInfoKHR &buildSizes,
        bool fastTrace, bool update);

    void BuildTopLevel(VkCommandBuffer cmd);


    VkAccelerationStructureBuildSizesInfoKHR GetBuildSizes(
        VkAccelerationStructureTypeKHR type, uint32_t geometryCount,
        const VkAccelerationStructureGeometryKHR *pGeometries,
        const uint32_t *pMaxPrimitiveCount, bool fastTrace) const;

    // GetBuildSizes(..) for BLAS
    VkAccelerationStructureBuildSizesInfoKHR GetBottomBuildSizes(
        uint32_t geometryCount,
        const VkAccelerationStructureGeometryKHR *pGeometries,
        const uint32_t *pMaxPrimitiveCount, bool fastTrace) const;
    // GetBuildSizes(..) for TLAS
    VkAccelerationStructureBuildSizesInfoKHR GetTopBuildSizes(
        const VkAccelerationStructureGeometryKHR *pGeometry,
        const uint32_t *pMaxPrimitiveCount, bool fastTrace) const;

    bool IsEmpty() const;

private:
    VkDevice device;
    std::shared_ptr<ScratchBuffer> scratchBuffer;

    struct BuildInfo
    {
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> geomInfos;
        std::vector<const VkAccelerationStructureBuildRangeInfoKHR *> rangeInfos;
    };

    BuildInfo bottomLBuildInfo;
    BuildInfo topLBuildInfo;
};

}