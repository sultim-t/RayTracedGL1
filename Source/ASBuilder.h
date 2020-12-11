#pragma once

#include "Common.h"
#include "ScratchBuffer.h"

class ASBuilder
{
public:
    explicit ASBuilder(std::shared_ptr<ScratchBuffer> commonScratchBuffer);

    // TODO: change to *pGeometries when VkSDK 1.2.164 will be released
    // ppGeometries is a pointer to a pointer to an array of size "geometryCount",
    // offsetInfos is an array of size "geometryCount".
    // All pointers must be valid until BuildBottomLevel is called
    void AddBLAS(VkAccelerationStructureKHR as,
                 uint32_t geometryCount,
                 const VkAccelerationStructureGeometryKHR** ppGeometries,
                 const VkAccelerationStructureBuildOffsetInfoKHR *pOffsetInfos, 
                 bool fastTrace, bool update);
    //void AddTLAS(VkAccelerationStructureKHR as, VkAccelerationStructureGeometryKHR **ppGeometries, uint32_t geometryCount, bool fastTrace);

    void BuildBottomLevel(VkCommandBuffer cmd);

private:
    void SyncScratch(VkCommandBuffer cmd);

private:
    std::shared_ptr<ScratchBuffer> scratchBuffer;

    struct BuildInfo
    {
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> geomInfos;
        std::vector<const VkAccelerationStructureBuildOffsetInfoKHR*> offsetInfos;
    };

    BuildInfo bottomLBuildInfo;
    BuildInfo topLBuildInfo;
};