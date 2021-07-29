// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "GeomInfoManager.h"

#include <algorithm>

#include "Matrix.h"
#include "VertexCollectorFilterType.h"
#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"

constexpr uint32_t GEOM_INFO_BUFFER_SIZE = MAX_TOP_LEVEL_INSTANCE_COUNT * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(RTGL1::ShGeometryInstance);
constexpr uint32_t GEOM_INFO_MATCH_PREV_BUFFER_SIZE = MAX_TOP_LEVEL_INSTANCE_COUNT * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(int32_t);

static_assert(sizeof(RTGL1::ShGeometryInstance) % 16 == 0, "Std430 structs must be aligned by 16 bytes");

RTGL1::GeomInfoManager::GeomInfoManager(VkDevice _device, std::shared_ptr<MemoryAllocator> &_allocator)
:
    device(_device),
    staticGeomCount(0),
    dynamicGeomCount(0)
{
    buffer = std::make_shared<AutoBuffer>(device, _allocator, "Geometry info staging buffer", "Geometry info buffer");
    matchPrev = std::make_shared<AutoBuffer>(device, _allocator, "Match previous Geometry infos staging buffer", "Match previous Geometry infos buffer");

    buffer->Create(GEOM_INFO_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    matchPrev->Create(GEOM_INFO_MATCH_PREV_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    matchPrevShadow = std::make_unique<int32_t[]>(GEOM_INFO_MATCH_PREV_BUFFER_SIZE);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        copyRegionLowerBounds[i].resize(MAX_TOP_LEVEL_INSTANCE_COUNT, UINT32_MAX);
        copyRegionUpperBounds[i].resize(MAX_TOP_LEVEL_INSTANCE_COUNT, 0);
    }
}

RTGL1::GeomInfoManager::~GeomInfoManager()
{
}

bool RTGL1::GeomInfoManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, bool insertBarrier)
{
    CmdLabel label(cmd, "Copying geom infos");

    {
        VkBufferCopy copyInfos[MAX_TOP_LEVEL_INSTANCE_COUNT];
        VkBufferMemoryBarrier barriers[MAX_TOP_LEVEL_INSTANCE_COUNT];

        uint32_t infoCount = 0;

        for (auto cf : VertexCollectorFilterGroup_ChangeFrequency)
        {
            uint64_t size = cf & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ?
                matchPrevCopyInfo.maxDynamicGeomCount * sizeof(int32_t) :
                matchPrevCopyInfo.maxStaticGeomCount * sizeof(int32_t);

            if (size == 0)
            {
                continue;
            }

            for (auto pt : VertexCollectorFilterGroup_PassThrough)
            {
                for (auto pm : VertexCollectorFilterGroup_PrimaryVisibility)
                {
                    uint64_t offset = 
                        VertexCollectorFilterTypeFlags_ToOffset(cf | pt | pm) * 
                        MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(int32_t);

                    // copy to staging
                    {
                        uint8_t *pDst = (uint8_t*)matchPrev->GetMapped(frameIndex);
                        uint8_t *pSrc = (uint8_t*)matchPrevShadow.get();

                        memcpy(pDst + offset, pSrc + offset, size);
                    }

                    // copy from staging
                    {
                        VkBufferCopy &c = copyInfos[infoCount];

                        c = {};
                        c.srcOffset = offset;
                        c.dstOffset = offset;
                        c.size = size;
                    }

                    {
                        VkBufferMemoryBarrier &b = barriers[infoCount];

                        b = {};
                        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                        b.buffer = matchPrev->GetDeviceLocal();
                        b.offset = offset;
                        b.size = size;
                    }

                    infoCount++;
                }
            }
        }

        if (infoCount > 0)
        {
            matchPrev->CopyFromStaging(cmd, frameIndex, copyInfos, infoCount);

            if (insertBarrier)
            {
                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    0,
                    0, nullptr,
                    infoCount, barriers,
                    0, nullptr);
            }
        }
    }


    {
        VkBufferCopy copyInfos[MAX_TOP_LEVEL_INSTANCE_COUNT];
        VkBufferMemoryBarrier barriers[MAX_TOP_LEVEL_INSTANCE_COUNT];

        uint32_t infoCount = 0;

        for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
        {
            const uint32_t lower = copyRegionLowerBounds[frameIndex][type];
            const uint32_t upper = copyRegionUpperBounds[frameIndex][type];

            if (lower < upper)
            {
                const uint32_t typeOffset = MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * type;

                const uint64_t offset = sizeof(ShGeometryInstance) * (typeOffset + lower);
                const uint64_t size = sizeof(ShGeometryInstance) * (upper - lower);

                {
                    VkBufferCopy &c = copyInfos[infoCount];

                    c = {};
                    c.srcOffset = offset;
                    c.dstOffset = offset;
                    c.size = size;
                }

                {
                    VkBufferMemoryBarrier &b = barriers[infoCount];

                    b = {};
                    b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    b.buffer = buffer->GetDeviceLocal();
                    b.offset = offset;
                    b.size = size;
                }

                infoCount++;
            }
        }

        if (infoCount == 0)
        {
            return false;
        }

        buffer->CopyFromStaging(cmd, frameIndex, copyInfos, infoCount);

        if (insertBarrier)
        {
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0,
                0, nullptr,
                infoCount, barriers,
                0, nullptr);
        }
    }

    return true;
}

void RTGL1::GeomInfoManager::ResetMatchPrevForGroup(uint32_t frameIndex, VertexCollectorFilterTypeFlags groupFlags)
{
    int32_t *prevIndexToCurIndex = matchPrevShadow.get();

    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(groupFlags) * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT;
    int32_t *toReset = prevIndexToCurIndex + offset;

    uint32_t maxGeomCount = groupFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? dynamicGeomCount : staticGeomCount;

    // approximate exact size with dynamicGeomCount
    uint32_t resetCount = std::min(maxGeomCount, (uint32_t)MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT);

    // reset matchPrev data for dynamic
    memset(toReset, 0xFF, resetCount * sizeof(int32_t));
}

void RTGL1::GeomInfoManager::ResetOnlyDynamic(uint32_t frameIndex)
{
    typedef VertexCollectorFilterTypeFlags FL;
    typedef VertexCollectorFilterTypeFlagBits FT;

    // do nothing, if there were no dynamic indices
    if (dynamicGeomCount > 0)
    {
        // trim before static indices
        if (staticGeomCount > 0)
        {
            geomType.resize(staticGeomCount);
            simpleToLocalIndex.resize(staticGeomCount);
        }
        else
        {
            geomType.clear();
            simpleToLocalIndex.clear();
        }

        // reset each dynamic group
        for (auto pt : VertexCollectorFilterGroup_PassThrough)
        {
            for (auto pm : VertexCollectorFilterGroup_PrimaryVisibility)
            {
                ResetMatchPrevForGroup(frameIndex, FT::CF_DYNAMIC | pt | pm);
            }
        }

        // reset only dynamic count
        dynamicGeomCount = 0;
    }

    for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
    {
        std::fill(copyRegionLowerBounds[frameIndex].begin(), copyRegionLowerBounds[frameIndex].end(), UINT32_MAX);
        std::fill(copyRegionUpperBounds[frameIndex].begin(), copyRegionUpperBounds[frameIndex].end(), 0);
    }
}

void RTGL1::GeomInfoManager::ResetWithStatic()
{
    movableIDToGeomFrameInfo.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        // reset each group
        for (auto cf : VertexCollectorFilterGroup_ChangeFrequency)
        {
            for (auto pt : VertexCollectorFilterGroup_PassThrough)
            {
                for (auto pm : VertexCollectorFilterGroup_PrimaryVisibility)
                {
                    ResetMatchPrevForGroup(i, cf | pt | pm);
                }
            }
        }
    }

    staticGeomCount = 0;
    dynamicGeomCount = 0;

    geomType.clear();
    simpleToLocalIndex.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ResetOnlyDynamic(i);
    }
}

uint32_t RTGL1::GeomInfoManager::GetGlobalGeomIndex(uint32_t localGeomIndex, VertexCollectorFilterTypeFlags flags)
{
    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(flags);
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    return offset * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT + localGeomIndex;
}

RTGL1::ShGeometryInstance * RTGL1::GeomInfoManager::GetGeomInfoAddressByGlobalIndex(uint32_t frameIndex, uint32_t globalGeomIndex)
{
    auto *mapped = (ShGeometryInstance *)buffer->GetMapped(frameIndex);

    return &mapped[globalGeomIndex];
}

uint32_t RTGL1::GeomInfoManager::ConvertSimpleIndexToGlobal(uint32_t simpleIndex) const
{
    // must exist
    assert(simpleIndex < geomType.size());
    assert(geomType.size() == simpleToLocalIndex.size());

    const VertexCollectorFilterTypeFlags flags = geomType[simpleIndex];
    const uint32_t localGeomIndex = simpleToLocalIndex[simpleIndex];

    return GetGlobalGeomIndex(localGeomIndex, flags);
}

void RTGL1::GeomInfoManager::PrepareForFrame(uint32_t frameIndex)
{
    // save counts before resetting
    matchPrevCopyInfo.maxDynamicGeomCount = dynamicGeomCount;
    matchPrevCopyInfo.maxStaticGeomCount = staticGeomCount;

    dynamicIDToGeomFrameInfo[frameIndex].clear();
    ResetOnlyDynamic(frameIndex);
}

uint32_t RTGL1::GeomInfoManager::WriteGeomInfo(
    uint32_t frameIndex,
    uint64_t geomUniqueID,
    uint32_t localGeomIndex,
    VertexCollectorFilterTypeFlags flags,
    ShGeometryInstance &src)
{
    // must be aligned for per-triangle vertex attributes
    assert(src.baseVertexIndex % 3 == 0);
    assert(src.baseIndexIndex % 3 == 0);

    const uint32_t simpleIndex = GetCount();

    // must not exist
    assert(simpleIndex == geomType.size());
    assert(geomType.size() == simpleToLocalIndex.size());

    geomType.push_back(flags);
    simpleToLocalIndex.push_back(localGeomIndex);

    uint32_t frameBegin = frameIndex;
    uint32_t frameEnd = frameIndex + 1;

    bool isStatic = !(flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC);

    if (isStatic)
    {
        // no dynamic geoms before static ones
        assert(dynamicGeomCount == 0);

        // make sure that indices are added sequentially
        assert(staticGeomCount == simpleIndex);
        staticGeomCount++;

        // copy to all staging buffers
        frameBegin = 0;
        frameEnd = MAX_FRAMES_IN_FLIGHT;
    }
    else
    {
        // dynamic geoms only after static ones
        assert(staticGeomCount <= simpleIndex);

        assert(dynamicGeomCount == simpleIndex - staticGeomCount);
        dynamicGeomCount++;
    }

    uint32_t globalGeomIndex = GetGlobalGeomIndex(localGeomIndex, flags);

    uint32_t flagsOffset = VertexCollectorFilterTypeFlags_ToOffset(flags);

    for (uint32_t i = frameBegin; i < frameEnd; i++)
    {
        FillWithPrevFrameData(flags, geomUniqueID, globalGeomIndex, src, i);

        ShGeometryInstance *dst = GetGeomInfoAddressByGlobalIndex(i, globalGeomIndex);
        memcpy(dst, &src, sizeof(ShGeometryInstance));

        MarkGeomInfoIndexToCopy(i, localGeomIndex, flagsOffset);
    }

    WriteInfoForNextUsage(flags, geomUniqueID, globalGeomIndex, src, frameIndex);        

    return simpleIndex;
}

void RTGL1::GeomInfoManager::MarkGeomInfoIndexToCopy(uint32_t frameIndex, uint32_t localGeomIndex, uint32_t offset)
{
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    copyRegionLowerBounds[frameIndex][offset] = std::min(localGeomIndex,     copyRegionLowerBounds[frameIndex][offset]);
    copyRegionUpperBounds[frameIndex][offset] = std::max(localGeomIndex + 1, copyRegionUpperBounds[frameIndex][offset]);
}

void RTGL1::GeomInfoManager::FillWithPrevFrameData(
    VertexCollectorFilterTypeFlags flags, uint64_t geomUniqueID, 
    uint32_t currentGlobalGeomIndex, ShGeometryInstance &dst, int32_t frameIndex)
{
    int32_t *prevIndexToCurIndex = matchPrevShadow.get();

    const std::map<uint64_t, GeomFrameInfo> *prevIdToInfo = nullptr;

    bool isMovable = flags & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE;
    bool isDynamic = flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    // fill prev info, but only for movable and dynamic geoms
    if (isDynamic)
    {
        static_assert(MAX_FRAMES_IN_FLIGHT == 2, "Assuming MAX_FRAMES_IN_FLIGHT==2");
        uint32_t prevFrame = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

        prevIdToInfo = &dynamicIDToGeomFrameInfo[prevFrame];
    }
    else 
    {
        // global geom indices are not changing for static geometry
        prevIndexToCurIndex[currentGlobalGeomIndex] = (int32_t)currentGlobalGeomIndex;

        if (isMovable)
        {
            prevIdToInfo = &movableIDToGeomFrameInfo; 
        }
        else
        {
            MarkNoPrevInfo(dst);
            return;
        }
    }

    const auto prev = prevIdToInfo->find(geomUniqueID);

    // if no previous info
    if (prev == prevIdToInfo->end())
    {
        MarkNoPrevInfo(dst);
        return;
    }

    // if counts are not the same
    if (prev->second.vertexCount != dst.vertexCount || 
        prev->second.indexCount != dst.indexCount)
    {
        MarkNoPrevInfo(dst);
        return;
    }

    // copy data from previous frame to current ShGeometryInstance
    dst.prevBaseVertexIndex = prev->second.baseVertexIndex;
    dst.prevBaseIndexIndex = prev->second.baseIndexIndex;
    memcpy(dst.prevModel, prev->second.model, sizeof(float) * 16);

    if (isDynamic)
    {
        // save index to access ShGeometryInfo using previous frame's global geom index
        prevIndexToCurIndex[prev->second.prevGlobalGeomIndex] = currentGlobalGeomIndex;
    }
}

void RTGL1::GeomInfoManager::MarkNoPrevInfo(ShGeometryInstance &dst)
{
    dst.prevBaseVertexIndex = UINT32_MAX;
}

void RTGL1::GeomInfoManager::MarkMovableHasPrevInfo(ShGeometryInstance &dst)
{
    dst.prevBaseVertexIndex = dst.baseVertexIndex;
}

void RTGL1::GeomInfoManager::WriteInfoForNextUsage(
    VertexCollectorFilterTypeFlags flags, uint64_t geomUniqueID, 
    uint32_t currentGlobalGeomIndex, const ShGeometryInstance &src, int32_t frameIndex)
{
    bool isMovable = flags & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE;
    bool isDynamic = flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    std::map<uint64_t, GeomFrameInfo> *idToInfo = nullptr;

    if (isDynamic)
    {
        idToInfo = &dynamicIDToGeomFrameInfo[frameIndex];    
    }
    else if (isMovable)
    {
        idToInfo = &movableIDToGeomFrameInfo; 
    }
    else
    {
        return;
    }

    // IDs must be unique
    assert(idToInfo->find(geomUniqueID) == idToInfo->end());

    GeomFrameInfo f = {};
    memcpy(f.model, src.model, sizeof(float) * 16);
    f.baseVertexIndex = src.baseVertexIndex;
    f.baseIndexIndex = src.baseIndexIndex;
    f.vertexCount = src.vertexCount;
    f.indexCount = src.indexCount;
    f.prevGlobalGeomIndex = currentGlobalGeomIndex;

    (*idToInfo)[geomUniqueID] = f;
}

void RTGL1::GeomInfoManager::WriteStaticGeomInfoMaterials(uint32_t simpleIndex, uint32_t layer, const MaterialTextures &src)
{
    // only static
    // geoms are allowed to rewrite material info
    assert(simpleIndex < geomType.size());
    assert(!(geomType[simpleIndex] & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC));
    assert(geomType.size() == simpleToLocalIndex.size());

    const uint32_t flagsOffset = VertexCollectorFilterTypeFlags_ToOffset(geomType[simpleIndex]);
    const uint32_t globalIndex = ConvertSimpleIndexToGlobal(simpleIndex);

    // need to write to both staging buffers for static geometry
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ShGeometryInstance *dst = GetGeomInfoAddressByGlobalIndex(i, globalIndex);

        // copy new material info
        memcpy(dst->materials[layer], src.indices, TEXTURES_PER_MATERIAL_COUNT * sizeof(uint32_t));

        // mark to be copied
        MarkGeomInfoIndexToCopy(i, simpleToLocalIndex[simpleIndex], flagsOffset);
    }
}

void RTGL1::GeomInfoManager::WriteStaticGeomInfoTransform(uint32_t simpleIndex, uint64_t geomUniqueID, const RgTransform &src)
{
    if (simpleIndex >= geomType.size())
    {
        assert(0);
        return;
    }

    assert(geomType.size() == simpleToLocalIndex.size());


    const auto flags = geomType[simpleIndex];
    const uint32_t flagsOffset = VertexCollectorFilterTypeFlags_ToOffset(flags);

    // only static and movable
    // geoms are allowed to update transforms
    if (!(flags & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE))
    {
        assert(0);
        return;
    }


    float modelMatix[16];
    Matrix::ToMat4Transposed(modelMatix, src);

    auto prev = movableIDToGeomFrameInfo.find(geomUniqueID);

    // if movable is updated, then it must be added previously
    if (prev == movableIDToGeomFrameInfo.end())
    {
        assert(0);
        return;
    }

    float *prevModelMatrix = prev->second.model;

    const uint32_t localGeomIndex = simpleToLocalIndex[simpleIndex];
    const uint32_t globalIndex = GetGlobalGeomIndex(localGeomIndex, flags);

    // need to write to both staging buffers for static geometry
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ShGeometryInstance *dst = GetGeomInfoAddressByGlobalIndex(i, globalIndex);

        memcpy(dst->model, modelMatix, 16 * sizeof(float));
        memcpy(dst->prevModel, prevModelMatrix, 16 * sizeof(float));

        // mark that movable has a previous info now
        MarkMovableHasPrevInfo(*dst);

        // mark to be copied
        MarkGeomInfoIndexToCopy(i, localGeomIndex, flagsOffset);
    }


    // save new prev data
    memcpy(prevModelMatrix, modelMatix, 16 * sizeof(float));
}

uint32_t RTGL1::GeomInfoManager::GetCount() const
{
    return staticGeomCount + dynamicGeomCount;
}

uint32_t RTGL1::GeomInfoManager::GetStaticCount() const
{
    return staticGeomCount;
}

uint32_t RTGL1::GeomInfoManager::GetDynamicCount() const
{
    return dynamicGeomCount;
}

VkBuffer RTGL1::GeomInfoManager::GetBuffer() const
{
    return buffer->GetDeviceLocal();
}

VkBuffer RTGL1::GeomInfoManager::GetMatchPrevBuffer() const
{
    return matchPrev->GetDeviceLocal();
}

uint32_t RTGL1::GeomInfoManager::GetStaticGeomBaseVertexIndex(uint32_t simpleIndex)
{
    // just use frame 0, as infos have same values in both staging buffers
    return GetGeomInfoAddressByGlobalIndex(0, ConvertSimpleIndexToGlobal(simpleIndex))->baseVertexIndex;
}
