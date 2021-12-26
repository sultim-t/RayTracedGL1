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

#include "TriangleInfoManager.h"
#include "Generated/ShaderCommonC.h"

constexpr VkDeviceSize TRIANGLE_INFO_SIZE = sizeof(uint32_t);

RTGL1::TriangleInfoManager::TriangleInfoManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> &_allocator,
    std::shared_ptr<SectorVisibility> _sectorVisibility)
:
    device(_device),
    sectorVisibility(std::move(_sectorVisibility)),
    staticGeometryRange(0),
    dynamicGeometryRange(0),
    copyStaticRange(false)
{
    triangleSectorIndicesBuffer = std::make_unique<AutoBuffer>(device, _allocator, "Triangle info staging buffer", "Triangle info buffer");
    triangleSectorIndicesBuffer->Create(MAX_INDEXED_PRIMITIVE_COUNT * TRIANGLE_INFO_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    static_assert(sizeof(decltype(tempValues)::value_type) == TRIANGLE_INFO_SIZE, "");
}

RTGL1::TriangleInfoManager::~TriangleInfoManager()
{}

uint32_t RTGL1::TriangleInfoManager::UploadAndGetArrayIndex(uint32_t frameIndex, const uint32_t *pTriangleSectorIDs, uint32_t count, RgGeometryType geomType)
{
    if (pTriangleSectorIDs == nullptr || count == 0)
    {
        return GEOM_INST_NO_TRIANGLE_INFO;
    }

    if (geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
    {
        assert(0 && "Static movable triangle info (vertex/index arrays) should be uploaded only once. "
                    "However, if movable geometry moved, sector IDs are invalid. So we need to enforce new pTriangleSectorIDs "
                    "on movable geometry transform change. It's not implemented.\n"
                    "Another solution is to assume that dynamic/movable objects are smaller than sector, so whole "
                    "geometry has only one sector ID, and for movable ShGeometryInstance can be updated along with its new transform");
        return GEOM_INST_NO_TRIANGLE_INFO;
    }


    auto &indices = TransformIdsToIndices(pTriangleSectorIDs, count);


    uint32_t startIndexInArray;

    if (geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        // trying to add first dynamic, lock static
        if (dynamicGeometryRange.GetCount() == 0)
        {
            staticGeometryRange.Lock();
        }
        // to add dynamic, static must be already locked
        assert(staticGeometryRange.IsLocked());


        startIndexInArray = dynamicGeometryRange.GetFirstIndexAfterRange();

        uint32_t *pDst = (uint32_t *)triangleSectorIndicesBuffer->GetMapped(frameIndex);
        memcpy(&pDst[startIndexInArray], indices.data(), indices.size() * TRIANGLE_INFO_SIZE);

        dynamicGeometryRange.Add(indices.size());
    }
    else
    {
        startIndexInArray = staticGeometryRange.GetFirstIndexAfterRange();

        // need to copy static geom data to both staging buffers, to be able to upload it in any frameIndex
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++)
        {
            uint32_t *pDst = (uint32_t *)triangleSectorIndicesBuffer->GetMapped(f);
            memcpy(&pDst[startIndexInArray], indices.data(), indices.size() * TRIANGLE_INFO_SIZE);
        }

        staticGeometryRange.Add(indices.size());


        // update dynamic, as it should start right after static
        dynamicGeometryRange.StartIndexingAfter(staticGeometryRange);
    }


    indices.clear();
    return startIndexInArray;
}

void RTGL1::TriangleInfoManager::PrepareForFrame(uint32_t frameIndex)
{
    // start dynamic again, but don't touch static geom indices
    dynamicGeometryRange.Reset(0);
    dynamicGeometryRange.StartIndexingAfter(staticGeometryRange);
}

void RTGL1::TriangleInfoManager::Reset()
{
    staticGeometryRange.Reset(0);
    dynamicGeometryRange.Reset(0);
    copyStaticRange = true;
}

std::vector<RTGL1::SectorArrayIndex::index_t> &RTGL1::TriangleInfoManager::TransformIdsToIndices(const uint32_t *pTriangleSectorIDs, uint32_t count)
{
    assert(tempValues.empty());
    tempValues.reserve(count);

    for (uint32_t i = 0; i < count; i++)
    {
        SectorID id = SectorID{ pTriangleSectorIDs[i] };

        tempValues.push_back(sectorVisibility->SectorIDToArrayIndex(id).GetArrayIndex());
    }

    return tempValues;
}


bool RTGL1::TriangleInfoManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, bool insertBarrier)
{
    VkBufferCopy copyInfos[2] = {};
    uint32_t cc = 0;

    if (staticGeometryRange.GetCount() > 0 && copyStaticRange)
    {
        copyInfos[cc].srcOffset = copyInfos[cc].dstOffset = staticGeometryRange.GetStartIndex() * TRIANGLE_INFO_SIZE;
        copyInfos[cc].size = staticGeometryRange.GetCount() * TRIANGLE_INFO_SIZE;
        cc++;
    }
    if (dynamicGeometryRange.GetCount() > 0)
    {
        copyInfos[cc].srcOffset = copyInfos[cc].dstOffset = dynamicGeometryRange.GetStartIndex() * TRIANGLE_INFO_SIZE;
        copyInfos[cc].size = dynamicGeometryRange.GetCount() * TRIANGLE_INFO_SIZE;
        cc++;
    }
    triangleSectorIndicesBuffer->CopyFromStaging(cmd, frameIndex, copyInfos, cc);


    if (insertBarrier)
    {
        VkBufferMemoryBarrier barriers[2] = {};
        uint32_t bc = 0;
        
        if (staticGeometryRange.GetCount() > 0 && copyStaticRange)
        {
            auto &b = barriers[bc];

            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            b.buffer = triangleSectorIndicesBuffer->GetDeviceLocal();
            b.offset = staticGeometryRange.GetStartIndex() * TRIANGLE_INFO_SIZE;
            b.size   = staticGeometryRange.GetCount() * TRIANGLE_INFO_SIZE;

            bc++;
        }
        if (dynamicGeometryRange.GetCount() > 0)
        {
            auto &b = barriers[bc];

            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            b.buffer = triangleSectorIndicesBuffer->GetDeviceLocal();
            b.offset = dynamicGeometryRange.GetStartIndex() * TRIANGLE_INFO_SIZE;
            b.size   = dynamicGeometryRange.GetCount() * TRIANGLE_INFO_SIZE;

            bc++;
        }
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            bc, barriers,
            0, nullptr);
    }


    copyStaticRange = false;
    return true;
}

VkBuffer RTGL1::TriangleInfoManager::GetBuffer() const
{
    return triangleSectorIndicesBuffer->GetDeviceLocal();
}
