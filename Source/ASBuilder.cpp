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

#include "ASBuilder.h"
#include <utility>

#include "Utils.h"

using namespace RTGL1;

ASBuilder::ASBuilder(VkDevice device, std::shared_ptr<ScratchBuffer> commonScratchBuffer) :
    scratchBuffer(std::move(commonScratchBuffer))
{
    this->device = device;
}

VkAccelerationStructureBuildSizesInfoKHR ASBuilder::GetBuildSizes(
    VkAccelerationStructureTypeKHR type,
    uint32_t geometryCount, 
    const VkAccelerationStructureGeometryKHR *pGeometries,
    const uint32_t *pMaxPrimitiveCount, 
    bool fastTrace) const
{
    assert(geometryCount > 0);

    // mode, srcAccelerationStructure, dstAccelerationStructure
    // and all VkDeviceOrHostAddressKHR except transformData are ignored
    // in vkGetAccelerationStructureBuildSizesKHR(..)
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = type;
    buildInfo.flags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    buildInfo.geometryCount = geometryCount;
    buildInfo.pGeometries = pGeometries;
    buildInfo.ppGeometries = nullptr;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    svkGetAccelerationStructureBuildSizesKHR(
        device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, pMaxPrimitiveCount, &sizeInfo);

    return sizeInfo;
}

VkAccelerationStructureBuildSizesInfoKHR ASBuilder::GetBottomBuildSizes(
    uint32_t geometryCount,
    const VkAccelerationStructureGeometryKHR *pGeometries, const uint32_t *pMaxPrimitiveCount, bool fastTrace) const
{
    return GetBuildSizes(
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, geometryCount,
        pGeometries, pMaxPrimitiveCount, fastTrace);
}

VkAccelerationStructureBuildSizesInfoKHR ASBuilder::GetTopBuildSizes(
    const VkAccelerationStructureGeometryKHR *pGeometry, const uint32_t *pMaxPrimitiveCount, bool fastTrace)  const
{
    return GetBuildSizes(
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 1,
        pGeometry, pMaxPrimitiveCount, fastTrace);
}

void ASBuilder::AddBLAS(
    VkAccelerationStructureKHR as, uint32_t geometryCount,
    const VkAccelerationStructureGeometryKHR* pGeometries,
    const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfos,
    const VkAccelerationStructureBuildSizesInfoKHR &buildSizes,
    bool fastTrace, bool update)
{
    // while building bottom level, top level must be not
    assert(topLBuildInfo.geomInfos.empty() && topLBuildInfo.rangeInfos.empty());

    assert(geometryCount > 0);

    VkDeviceSize scratchSize = buildSizes.buildScratchSize;

    // use bigger scratch size, if need to update
    if (update && scratchSize < buildSizes.updateScratchSize)
    {
        scratchSize = buildSizes.updateScratchSize;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    buildInfo.mode = (VkBuildAccelerationStructureModeKHR)0;
    buildInfo.srcAccelerationStructure = update ? as : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = scratchBuffer->GetScratchAddress(scratchSize);
    buildInfo.geometryCount = geometryCount;
    buildInfo.pGeometries = pGeometries;
    buildInfo.ppGeometries = nullptr;

    bottomLBuildInfo.geomInfos.push_back(buildInfo);
    bottomLBuildInfo.rangeInfos.push_back(pRangeInfos);
}

void ASBuilder::BuildBottomLevel(VkCommandBuffer cmd)
{
    assert(bottomLBuildInfo.geomInfos.size() == bottomLBuildInfo.rangeInfos.size());
    assert(!bottomLBuildInfo.geomInfos.empty());

    // build bottom level
    svkCmdBuildAccelerationStructuresKHR(cmd, bottomLBuildInfo.geomInfos.size(), 
                                        bottomLBuildInfo.geomInfos.data(), bottomLBuildInfo.rangeInfos.data());

    // sync scratch buffer and AS access
    Utils::ASBuildMemoryBarrier(cmd);
    scratchBuffer->Reset();

    bottomLBuildInfo.geomInfos.clear();
    bottomLBuildInfo.rangeInfos.clear();
}

void ASBuilder::AddTLAS(
    VkAccelerationStructureKHR as,
    const VkAccelerationStructureGeometryKHR *pGeometry,
    const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfo,
    const VkAccelerationStructureBuildSizesInfoKHR &buildSizes,
    bool fastTrace, bool update)
{
    // while building top level, bottom level must be not
    assert(topLBuildInfo.geomInfos.empty() && topLBuildInfo.rangeInfos.empty());

    VkDeviceSize scratchSize = update ? buildSizes.updateScratchSize : buildSizes.buildScratchSize;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    buildInfo.mode = update ?
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = update ? as : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = scratchBuffer->GetScratchAddress(scratchSize);
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = pGeometry;
    buildInfo.ppGeometries = nullptr;

    topLBuildInfo.geomInfos.push_back(buildInfo);
    topLBuildInfo.rangeInfos.push_back(pRangeInfo);
}

void ASBuilder::BuildTopLevel(VkCommandBuffer cmd)
{
    assert(topLBuildInfo.geomInfos.size() == topLBuildInfo.rangeInfos.size());
    assert(!topLBuildInfo.geomInfos.empty());

    // build bottom level
    svkCmdBuildAccelerationStructuresKHR(cmd, topLBuildInfo.geomInfos.size(),
                                        topLBuildInfo.geomInfos.data(), topLBuildInfo.rangeInfos.data());

    // sync scratch buffer and AS access
    Utils::ASBuildMemoryBarrier(cmd);
    scratchBuffer->Reset();

    topLBuildInfo.geomInfos.clear();
    topLBuildInfo.rangeInfos.clear();
}

bool ASBuilder::IsEmpty() const
{
    return bottomLBuildInfo.geomInfos.empty() && bottomLBuildInfo.rangeInfos.empty() &&
        topLBuildInfo.geomInfos.empty() && topLBuildInfo.rangeInfos.empty();
}