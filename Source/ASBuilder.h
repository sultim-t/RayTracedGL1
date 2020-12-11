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
    void AddBLAS(
        VkAccelerationStructureKHR as,
        uint32_t geometryCount,
        const VkAccelerationStructureGeometryKHR **ppGeometries,
        const VkAccelerationStructureBuildOffsetInfoKHR *pOffsetInfos,
        bool fastTrace, bool update);

    void BuildBottomLevel(VkCommandBuffer cmd);


    void AddTLAS(
        VkAccelerationStructureKHR as,
        const VkAccelerationStructureGeometryKHR **ppGeometry,
        const VkAccelerationStructureBuildOffsetInfoKHR *offsetInfo,
        bool fastTrace, bool update);

    void BuildTopLevel(VkCommandBuffer cmd);


    bool IsEmpty() const;

private:
    void SyncScratch(VkCommandBuffer cmd);

private:
    std::shared_ptr<ScratchBuffer> scratchBuffer;

    struct BuildInfo
    {
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> geomInfos;
        std::vector<const VkAccelerationStructureBuildOffsetInfoKHR *> offsetInfos;
    };

    BuildInfo bottomLBuildInfo;
    BuildInfo topLBuildInfo;
};