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

#include "Matrix.h"
#include "VertexCollectorFilterType.h"
#include "Generated/ShaderCommonC.h"

constexpr uint32_t GEOM_INFO_BUFFER_SIZE = MAX_TOP_LEVEL_INSTANCE_COUNT * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(RTGL1::ShGeometryInstance);

static_assert(sizeof(RTGL1::ShGeometryInstance) % 16 == 0, "Std430 structs must be aligned by 16 bytes");

RTGL1::GeomInfoManager::GeomInfoManager(VkDevice _device, std::shared_ptr<MemoryAllocator> _allocator)
:
    device(_device),
    staticGeomCount(0),
    dynamicGeomCount(0),
    copyRegionLowerBound{},
    copyRegionUpperBound{}
{
    buffer = std::make_shared<AutoBuffer>(device, std::move(_allocator), "Geometry info staging buffer", "Geometry info buffer");

    buffer->Create(
        GEOM_INFO_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    static_assert(
        sizeof(copyRegionLowerBound[0]) / sizeof(copyRegionLowerBound[0][0]) == MAX_TOP_LEVEL_INSTANCE_COUNT &&
        sizeof(copyRegionUpperBound[0]) / sizeof(copyRegionUpperBound[0][0]) == MAX_TOP_LEVEL_INSTANCE_COUNT,
        "Number of geomInfo copy regions must be MAX_TOP_LEVEL_INSTANCE_COUNT");

    memset(copyRegionLowerBound, 0xFF, sizeof(copyRegionLowerBound));
}

RTGL1::GeomInfoManager::~GeomInfoManager()
{
}

bool RTGL1::GeomInfoManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, bool insertBarrier)
{
    VkBufferCopy infos[MAX_TOP_LEVEL_INSTANCE_COUNT];
    uint32_t infoCount = 0;

    for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
    {
        const uint32_t lower = copyRegionLowerBound[frameIndex][type];
        const uint32_t upper = copyRegionUpperBound[frameIndex][type];

        if (lower < upper)
        {
            const uint32_t typeOffset = MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * type;

            const uint64_t offset = sizeof(ShGeometryInstance) * (typeOffset + lower);
            const uint64_t size = sizeof(ShGeometryInstance) * (upper - lower);

            infos[infoCount].srcOffset = offset;
            infos[infoCount].dstOffset = offset;
            infos[infoCount].size = size;

            infoCount++;
        }
    }

    if (infoCount == 0)
    {
        return false;
    }

    buffer->CopyFromStaging(cmd, frameIndex, infos, infoCount);

    if (insertBarrier)
    {
        VkBufferMemoryBarrier gmtBr = {};

        gmtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        gmtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        gmtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        gmtBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        gmtBr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        gmtBr.buffer = buffer->GetDeviceLocal();
        gmtBr.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            1, &gmtBr,
            0, nullptr);
    }

    return true;
}

void RTGL1::GeomInfoManager::ResetOnlyDynamic(uint32_t frameIndex)
{
    // do nothing, if there were no dynamic indices
    if (dynamicGeomCount > 0)
    {
        // trim before static indices
        if (staticGeomCount > 0)
        {
            geomType.resize(staticGeomCount);
            globalToLocalIndex.resize(staticGeomCount);
        }
        else
        {
            geomType.clear();
            globalToLocalIndex.clear();
        }

        // reset only dynamic count
        dynamicGeomCount = 0;
    }

    for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
    {
        memset(copyRegionLowerBound[frameIndex], 0xFF,  sizeof(copyRegionLowerBound[frameIndex]));
        memset(copyRegionUpperBound[frameIndex], 0,     sizeof(copyRegionUpperBound[frameIndex]));
    }
}

void RTGL1::GeomInfoManager::ResetWithStatic()
{
    movableIDToGeomFrameInfo.clear();

    staticGeomCount = 0;
    dynamicGeomCount = 0;

    geomType.clear();
    globalToLocalIndex.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ResetOnlyDynamic(i);
    }
}


RTGL1::ShGeometryInstance *RTGL1::GeomInfoManager::GetGeomInfoAddress(uint32_t frameIndex, uint32_t geomIndex)
{
    // must exist
    assert(geomIndex < geomType.size());
    assert(geomType.size() == globalToLocalIndex.size());

    VertexCollectorFilterTypeFlags flags = geomType[geomIndex];
    uint32_t localGeomIndex = globalToLocalIndex[geomIndex];

    return GetGeomInfoAddress(frameIndex, localGeomIndex, flags);
}

RTGL1::ShGeometryInstance *RTGL1::GeomInfoManager::GetGeomInfoAddress(uint32_t frameIndex, uint32_t localGeomIndex, VertexCollectorFilterTypeFlags flags)
{
    auto *mapped = (ShGeometryInstance *)buffer->GetMapped(frameIndex);

    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(flags);
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    return &mapped[offset * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT + localGeomIndex];
}

void RTGL1::GeomInfoManager::PrepareForFrame(uint32_t frameIndex)
{
    dynamicIDToGeomFrameInfo[frameIndex].clear();
}

uint32_t RTGL1::GeomInfoManager::WriteGeomInfo(
    uint32_t frameIndex,
    uint64_t geomUniqueID,
    uint32_t localGeomIndex,
    VertexCollectorFilterTypeFlags flags,
    ShGeometryInstance &src)
{
    const uint32_t globalGeomIndex = GetCount();

    // must not exist
    assert(globalGeomIndex == geomType.size());
    assert(geomType.size() == globalToLocalIndex.size());

    geomType.push_back(flags);
    globalToLocalIndex.push_back(localGeomIndex);

    uint32_t frameBegin = frameIndex;
    uint32_t frameEnd = frameIndex + 1;

    bool isStatic = !(flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC);

    if (isStatic)
    {
        // no dynamic geoms before static ones
        assert(dynamicGeomCount == 0);

        // make sure that indices are added sequentially
        assert(staticGeomCount == globalGeomIndex);
        staticGeomCount++;

        // copy to all staging buffers
        frameBegin = 0;
        frameEnd = MAX_FRAMES_IN_FLIGHT;
    }
    else
    {
        // dynamic geoms only after static ones
        assert(staticGeomCount <= globalGeomIndex);

        assert(dynamicGeomCount == globalGeomIndex - staticGeomCount);
        dynamicGeomCount++;
    }

    for (uint32_t i = frameBegin; i < frameEnd; i++)
    {
        FillWithPrevFrameData(flags, geomUniqueID, src, frameIndex);

        ShGeometryInstance *dst = GetGeomInfoAddress(i, localGeomIndex, flags);
        memcpy(dst, &src, sizeof(ShGeometryInstance));

        MarkGeomInfoIndexToCopy(i, localGeomIndex, flags);
    }

    WriteInfoForNextUsage(flags, geomUniqueID, src, frameIndex);        

    return globalGeomIndex;
}

void RTGL1::GeomInfoManager::MarkGeomInfoIndexToCopy(uint32_t frameIndex, uint32_t localGeomIndex, VertexCollectorFilterTypeFlags flags)
{
    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(flags);
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    copyRegionLowerBound[frameIndex][offset] = std::min(localGeomIndex,     copyRegionLowerBound[frameIndex][offset]);
    copyRegionUpperBound[frameIndex][offset] = std::max(localGeomIndex + 1, copyRegionUpperBound[frameIndex][offset]);
}

void RTGL1::GeomInfoManager::FillWithPrevFrameData(VertexCollectorFilterTypeFlags flags, uint64_t geomUniqueID, ShGeometryInstance &dst, int32_t frameIndex)
{
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
    else if (isMovable)
    {
        prevIdToInfo = &movableIDToGeomFrameInfo; 
    }
    else
    {
        MarkNoPrevInfo(dst);
        return;
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
    
    dst.prevBaseVertexIndex = prev->second.baseVertexIndex;
    dst.prevBaseIndexIndex = prev->second.baseIndexIndex;
    memcpy(dst.prevModel, prev->second.model, sizeof(float) * 16);
}

void RTGL1::GeomInfoManager::MarkNoPrevInfo(ShGeometryInstance &dst)
{
    dst.prevBaseVertexIndex = UINT32_MAX;
}

void RTGL1::GeomInfoManager::MarkMovableHasPrevInfo(ShGeometryInstance &dst)
{
    dst.prevBaseVertexIndex = dst.baseVertexIndex;
}

void RTGL1::GeomInfoManager::WriteInfoForNextUsage(VertexCollectorFilterTypeFlags flags, uint64_t geomUniqueID, const ShGeometryInstance &src, int32_t frameIndex)
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

    (*idToInfo)[geomUniqueID] = f;
}

void RTGL1::GeomInfoManager::WriteStaticGeomInfoMaterials(uint32_t globalGeomIndex, uint32_t layer, const MaterialTextures &src)
{
    // only static
    // geoms are allowed to rewrite material info
    assert(globalGeomIndex < geomType.size());
    assert(!(geomType[globalGeomIndex] & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC));
    assert(geomType.size() == globalToLocalIndex.size());

    // need to write to both staging buffers for static geometry
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ShGeometryInstance *dst = GetGeomInfoAddress(i, globalGeomIndex);

        // copy new material info
        memcpy(dst->materials[layer], src.indices, TEXTURES_PER_MATERIAL_COUNT * sizeof(uint32_t));

        // mark to be copied
        MarkGeomInfoIndexToCopy(i, globalToLocalIndex[globalGeomIndex], geomType[globalGeomIndex]);
    }
}

void RTGL1::GeomInfoManager::WriteStaticGeomInfoTransform(uint32_t globalGeomIndex, uint64_t geomUniqueID, const RgTransform &src)
{
    if (globalGeomIndex >= geomType.size())
    {
        assert(0);
        return;
    }

    // only static and movable
    // geoms are allowed to update transforms
    if (!(geomType[globalGeomIndex] & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE))
    {
        assert(0);
        return;
    }

    assert(geomType.size() == globalToLocalIndex.size());


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


    // need to write to both staging buffers for static geometry
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ShGeometryInstance *dst = GetGeomInfoAddress(i, globalGeomIndex);

        memcpy(dst->model, modelMatix, 16 * sizeof(float));
        memcpy(dst->prevModel, prevModelMatrix, 16 * sizeof(float));

        // mark that movable has a previous info now
        MarkMovableHasPrevInfo(*dst);

        // mark to be copied
        MarkGeomInfoIndexToCopy(i, globalToLocalIndex[globalGeomIndex], geomType[globalGeomIndex]);
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

uint32_t RTGL1::GeomInfoManager::GetStaticGeomBaseVertexIndex(uint32_t globalGeomIndex)
{
    // just use frame 0, as infos have same values in both staging buffers
    return GetGeomInfoAddress(0, globalGeomIndex)->baseVertexIndex;
}