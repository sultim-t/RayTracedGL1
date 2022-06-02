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

#pragma once

#include <array>
#include <vector>

#include "AutoBuffer.h"
#include "LightDefs.h"
#include "SectorVisibility.h"
#include "RTGL1/RTGL1.h"

namespace RTGL1
{

class LightLists
{
public:
    LightLists(VkDevice device, 
               const std::shared_ptr<MemoryAllocator> &memoryAllocator,
               std::shared_ptr<SectorVisibility> sectorVisibility,
               const char *pDebugName);
    ~LightLists() = default;

    LightLists(const LightLists &other) = delete;
    LightLists(LightLists &&other) noexcept = delete;
    LightLists &operator=(const LightLists &other) = delete;
    LightLists &operator=(LightLists &&other) noexcept = delete;

    void PrepareForFrame();
    void Reset();

    void InsertLight(LightArrayIndex lightIndex, SectorArrayIndex lightSectorIndex);
    void BuildAndCopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex);

    SectorArrayIndex SectorIDToArrayIndex(SectorID id) const;

    VkBuffer GetPlainLightListDeviceLocalBuffer();
    VkBuffer GetSectorToLightListRegionDeviceLocalBuffer();

private:
    void AddLightToSectorLightList(LightArrayIndex lightIndex, SectorArrayIndex lightSectorIndex);

    void BuildArrays(
        LightArrayIndex::index_t *pOutputPlainLightList, uint32_t *pOutputPlainLightListSize,
        SectorArrayIndex::index_t *pOutputSectorToLightListStartEnd, uint32_t *pOutputSectorCountToCopy) const;

private:
    std::shared_ptr<SectorVisibility> sectorVisibility;

    // light list for each sector in the current frame,
    // assume that it's indexed by 'SectorArrayIndex'
    std::array<std::vector<LightArrayIndex>, MAX_SECTOR_COUNT> lightLists;

    std::shared_ptr<AutoBuffer> plainLightList;
    std::shared_ptr<AutoBuffer> sectorToLightListRegion;

    // used to copy to mapped memory, to reduce interactions with mapped memory
    std::vector<LightArrayIndex::index_t> plainLightList_Raw;
    std::vector<SectorArrayIndex::index_t> sectorToLightListRegion_Raw;
};

}