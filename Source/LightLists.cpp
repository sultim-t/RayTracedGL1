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

#include "LightLists.h"

#include <string>

#include "RgException.h"
#include "Generated/ShaderCommonC.h"


#define PLAIN_LIGHT_LIST_SIZEOF_ELEMENT             (sizeof(decltype(plainLightList_Raw)::value_type))
#define SECTOR_TO_LIGHT_LIST_REGION_SIZEOF_ELEMENT  (sizeof(decltype(sectorToLightListRegion_Raw)::value_type))

constexpr std::size_t VECTOR_START_CAPACITY = 128;


static_assert(RTGL1::MAX_SECTOR_COUNT < SECTOR_INDEX_NONE, "");


RTGL1::LightLists::LightLists(
    VkDevice _device, 
    const std::shared_ptr<MemoryAllocator> &_memoryAllocator, 
    std::shared_ptr<SectorVisibility> _sectorVisibility)
:
    sectorVisibility(std::move(_sectorVisibility))
{
    // plain global light list, to use in shaders
    plainLightList_Raw.resize(MAX_SECTOR_COUNT * MAX_LIGHT_LIST_SIZE);

    plainLightList = std::make_shared<AutoBuffer>(_device, _memoryAllocator);
    plainLightList->Create(plainLightList_Raw.size() * PLAIN_LIGHT_LIST_SIZEOF_ELEMENT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Light list buffer");


    // contains tuples (begin, end) for each sector
    sectorToLightListRegion_Raw.resize(MAX_SECTOR_COUNT * 2);

    sectorToLightListRegion = std::make_shared<AutoBuffer>(_device, _memoryAllocator);
    sectorToLightListRegion->Create(sectorToLightListRegion_Raw.size() * SECTOR_TO_LIGHT_LIST_REGION_SIZEOF_ELEMENT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Sector to light list region buffer");
}

void RTGL1::LightLists::PrepareForFrame()
{
    // clear the vectors but without deallocating; they can be reused,
    // since the static scene sectors most probably will be the same
    for (auto &v : lightLists)
    {
        v.clear();
    }
}

void RTGL1::LightLists::Reset()
{
    for (auto &v : lightLists)
    {
        v = {};
    }
}

void RTGL1::LightLists::AddLightToSectorLightList(LightArrayIndex lightIndex, SectorArrayIndex lightSectorIndex)
{
    auto &v = lightLists[lightSectorIndex.GetArrayIndex()];

    // guarantee capacity of >= VECTOR_START_CAPACITY
    v.reserve(VECTOR_START_CAPACITY);
    v.push_back(lightIndex);

    // values must be unique
    assert(std::count(v.cbegin(), v.cend(), lightIndex) == 1);
}


void RTGL1::LightLists::InsertLight(LightArrayIndex lightIndex, SectorArrayIndex lightSectorIndex)
{
    // sector is always visible from itself, so append the light unconditionally
    AddLightToSectorLightList(lightIndex, lightSectorIndex);


    if (sectorVisibility->ArePotentiallyVisibleSectorsExist(lightSectorIndex))
    {
        // for each potentially visible sector from "lightSectorIndex"

        for (SectorArrayIndex visibleSector : sectorVisibility->GetPotentiallyVisibleSectors(lightSectorIndex))
        {
            assert(visibleSector != lightSectorIndex);
            
            // append given light to light list of such sector
            AddLightToSectorLightList(lightIndex, visibleSector);
        }
    }
}

void RTGL1::LightLists::BuildAndCopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    uint32_t plainLightListSize, sectorCountToCopy;

    BuildArrays(plainLightList_Raw.data(), &plainLightListSize,
                sectorToLightListRegion_Raw.data(), &sectorCountToCopy);

    uint64_t plainLightList_Bytes = plainLightListSize * PLAIN_LIGHT_LIST_SIZEOF_ELEMENT;
    uint64_t sectorToLightListRegion_Bytes = 2 * sectorCountToCopy * SECTOR_TO_LIGHT_LIST_REGION_SIZEOF_ELEMENT;

    memcpy(plainLightList->GetMapped(frameIndex),           plainLightList_Raw.data(),          plainLightList_Bytes);
    memcpy(sectorToLightListRegion->GetMapped(frameIndex),  sectorToLightListRegion_Raw.data(), sectorToLightListRegion_Bytes);
    
    plainLightList->CopyFromStaging(cmd, frameIndex, plainLightList_Bytes);
    sectorToLightListRegion->CopyFromStaging(cmd, frameIndex, sectorToLightListRegion_Bytes);
}

void RTGL1::LightLists::BuildArrays(
    LightArrayIndex::index_t *pOutputPlainLightList, uint32_t *pOutputPlainLightListSize,
    SectorArrayIndex::index_t *pOutputSectorToLightListStartEnd, uint32_t *pOutputSectorCountToCopy) const
{
    uint32_t iter = 0;

    for (SectorArrayIndex::index_t _i = 0; _i < lightLists.size(); _i++)
    {
        // pretend like we iterate over SectorArrayIndex
        const SectorArrayIndex sectorIndex = SectorArrayIndex{ _i };


        const std::vector<LightArrayIndex> &sectorLightList = lightLists[sectorIndex.GetArrayIndex()];

        const uint32_t startArrayOffset = iter;
        const uint32_t endArrayOffset   = iter + (uint32_t)sectorLightList.size();

        // copy all potentially visible lights of this sector to the dedicated light list part
        for (const LightArrayIndex &i : sectorLightList)
        {
            if (iter - startArrayOffset >= MAX_LIGHT_LIST_SIZE)
            {
                assert(0);
                break;
            }

            pOutputPlainLightList[iter] = i.GetArrayIndex();
            iter++;
        }

        // write start/end, so the sector's light list can be accessed by sector array index
        pOutputSectorToLightListStartEnd[sectorIndex.GetArrayIndex() * 2 + 0] = startArrayOffset;
        pOutputSectorToLightListStartEnd[sectorIndex.GetArrayIndex() * 2 + 1] = endArrayOffset;
    }

    *pOutputPlainLightListSize = iter;
    *pOutputSectorCountToCopy = lightLists.size();
}

RTGL1::SectorArrayIndex RTGL1::LightLists::SectorIDToArrayIndex(SectorID id) const
{
    return sectorVisibility->SectorIDToArrayIndex(id);
}

VkBuffer RTGL1::LightLists::GetPlainLightListDeviceLocalBuffer()
{
    return plainLightList->GetDeviceLocal();
}

VkBuffer RTGL1::LightLists::GetSectorToLightListRegionDeviceLocalBuffer()
{
    return sectorToLightListRegion->GetDeviceLocal();
}
