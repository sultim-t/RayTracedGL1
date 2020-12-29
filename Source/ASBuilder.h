#pragma once
#include <vector>
#include "Common.h"
#include "ScratchBuffer.h"

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
    void SyncScratch(VkCommandBuffer cmd);

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