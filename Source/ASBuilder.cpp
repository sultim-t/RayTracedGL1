#include "ASBuilder.h"
#include <utility>

ASBuilder::ASBuilder(std::shared_ptr<ScratchBuffer> commonScratchBuffer) :
    scratchBuffer(std::move(commonScratchBuffer))
{}

void ASBuilder::AddBLAS(
    VkAccelerationStructureKHR as, uint32_t geometryCount,
    const VkAccelerationStructureGeometryKHR** ppGeometries,
    const VkAccelerationStructureBuildOffsetInfoKHR *offsetInfos, 
    bool fastTrace, bool update)
{
    // while building bottom level, top level must be not
    assert(topLBuildInfo.geomInfos.size() == topLBuildInfo.offsetInfos.size() == 0);

    VkBuildAccelerationStructureFlagsKHR buildFlags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = buildFlags;
    buildInfo.update = update ? VK_TRUE : VK_FALSE;
    buildInfo.srcAccelerationStructure = update ? as : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = scratchBuffer->GetScratchAddress(as, update);
    buildInfo.geometryCount = geometryCount;
    // buildInfo.pGeometries = *ppGeometries;
    buildInfo.geometryArrayOfPointers = VK_FALSE;
    buildInfo.ppGeometries = ppGeometries;

    bottomLBuildInfo.geomInfos.push_back(buildInfo);
    bottomLBuildInfo.offsetInfos.push_back(offsetInfos);
}

void ASBuilder::BuildBottomLevel(VkCommandBuffer cmd)
{
    assert(bottomLBuildInfo.geomInfos.size() == bottomLBuildInfo.offsetInfos.size());

    // build bottom level
    vksCmdBuildAccelerationStructureKHR(cmd, bottomLBuildInfo.geomInfos.size(), 
                                        bottomLBuildInfo.geomInfos.data(), bottomLBuildInfo.offsetInfos.data());

    // sync scratch buffer access
    SyncScratch(cmd);
        
    scratchBuffer->Reset();
    bottomLBuildInfo.geomInfos.clear();
    bottomLBuildInfo.offsetInfos.clear();
}

bool ASBuilder::IsEmpty() const
{
    return bottomLBuildInfo.geomInfos.empty() && bottomLBuildInfo.offsetInfos.empty() &&
        topLBuildInfo.geomInfos.empty() && topLBuildInfo.offsetInfos.empty();
}

void ASBuilder::SyncScratch(VkCommandBuffer cmd)
{
    VkMemoryBarrier memBarrier = {};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask =
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    memBarrier.dstAccessMask =
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
                         1, &memBarrier,
                         0, nullptr,
                         0, nullptr);
}
