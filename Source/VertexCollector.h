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
#include <array>
#include <map>
#include <vector>

#include "Buffer.h"
#include "Common.h"
#include "IMaterialDependency.h"
#include "Material.h"
#include "VertexBufferProperties.h"
#include "VertexCollectorFilter.h"
#include "RTGL1/RTGL1.h"

struct ShGeometryInstance;

// The class collects vertex data to buffers with shader struct types.
// Geometries are passed to the class by chunks and the result of collecting
// is a vertex buffer with ready data and infos for acceleration structure creation/building.
class VertexCollector : public IMaterialDependency
{
public:
    explicit VertexCollector(
        VkDevice device, const std::shared_ptr<MemoryAllocator> &allocator,
        VkDeviceSize bufferSize, const VertexBufferProperties &properties,
        VertexCollectorFilterTypeFlags filters);
    ~VertexCollector() override;

    VertexCollector(const VertexCollector& other) = delete;
    VertexCollector(VertexCollector&& other) noexcept = delete;
    VertexCollector& operator=(const VertexCollector& other) = delete;
    VertexCollector& operator=(VertexCollector&& other) noexcept = delete;

    void BeginCollecting();
    uint32_t AddGeometry(const RgGeometryUploadInfo &info, const MaterialTextures materials[MATERIALS_MAX_LAYER_COUNT]);
    void EndCollecting();

    // Clear data that was generated while collecting.
    // Should be called when blasGeometries is not needed anymore
    virtual void Reset();
    // Copy buffer from staging and set barrier
    // "isStatic" is required to determine what GLSL struct to use for copying
    void CopyFromStaging(VkCommandBuffer cmd, bool isStatic);

    // Update transform, mainly for movable static geometry as dynamic geometry
    // will be updated every frame and thus their transforms.
    void UpdateTransform(uint32_t geomIndex, const RgTransform &transform);

    // When material data is changed, this function is called
    void OnMaterialChange(uint32_t materialIndex, const MaterialTextures &newInfo) override;


    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;
    VkBuffer GetGeometryInfosBuffer() const;


    // Get primitive counts from filters. Null if corresponding filter wasn't found.
    const std::vector<uint32_t> *GetPrimitiveCounts(
        VertexCollectorFilterTypeFlagBits filter) const;

    // Get AS geometries data from filters. Null if corresponding filter wasn't found.
    const std::vector<VkAccelerationStructureGeometryKHR> *GetASGeometries(
        VertexCollectorFilterTypeFlagBits filter) const;

    // Get AS build range infos from filters. Null if corresponding filter wasn't found.
    const std::vector<VkAccelerationStructureBuildRangeInfoKHR> *GetASBuildRangeInfos(
        VertexCollectorFilterTypeFlagBits filter) const;

private:
    void CopyDataToStaging(const RgGeometryUploadInfo &info, uint32_t vertIndex, bool isStatic);

    bool CopyVertexDataFromStaging(VkCommandBuffer cmd, bool isStatic);
    bool CopyIndexDataFromStaging(VkCommandBuffer cmd);
    bool GetVertBufferCopyInfos(bool isStatic, std::array<VkBufferCopy, 4> &outInfos) const;

    void AddMaterialDependency(uint32_t geomIndex, uint32_t layer, uint32_t materialIndex);

    void InitFilters(VertexCollectorFilterTypeFlags flags);
    void AddFilter(VertexCollectorFilterTypeFlagBits filter);
    void PushPrimitiveCount(VertexCollectorFilterTypeFlags type, uint32_t primCount);
    void PushGeometry(VertexCollectorFilterTypeFlags type, const VkAccelerationStructureGeometryKHR &geom);
    void PushRangeInfo(VertexCollectorFilterTypeFlags type, const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo);
    uint32_t GetAllGeometryCount() const;

    static VertexCollectorFilterTypeFlags GetFilterTypeFlags(const RgGeometryUploadInfo &info);

private:
    struct MaterialRef
    {
        uint32_t geomIndex;
        uint32_t layer;
    };

private:
    VkDevice device;
    VertexBufferProperties properties;

    Buffer stagingVertBuffer;
    Buffer vertBuffer;

    uint8_t *mappedVertexData;
    uint32_t *mappedIndexData;
    VkTransformMatrixKHR *mappedTransformData;

    ShGeometryInstance *mappedGeomInfosData;

    Buffer stagingIndexBuffer;
    Buffer indexBuffer;
    Buffer transforms;

    // buffer for getting info for geometry in BLAS
    Buffer geomInfosBuffer;

    uint32_t curVertexCount;
    uint32_t curIndexCount;
    uint32_t curPrimitiveCount;
    uint32_t curGeometryCount;

    // material index to a list of () that have that material
    std::map<uint32_t, std::vector<MaterialRef>> materialDependencies;

    std::vector<std::shared_ptr<VertexCollectorFilter>> filters;
};
