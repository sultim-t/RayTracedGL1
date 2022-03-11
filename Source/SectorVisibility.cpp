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

#include "SectorVisibility.h"

#include <cassert>
#include <string>

#include "RgException.h"


// value, from which sector array indices should start
constexpr RTGL1::SectorArrayIndex::index_t  SECTOR_ARRAY_INDEX_BASE_VALUE = 0;


RTGL1::SectorVisibility::SectorVisibility() : lastSectorArrayIndex(SECTOR_ARRAY_INDEX_BASE_VALUE), sectorArrayIndexToID()
{
    Reset();
}

void RTGL1::SectorVisibility::SetPotentialVisibility(SectorID a, SectorID b)
{
    const SectorArrayIndex ia = AssignArrayIndexForID(a);
    const SectorArrayIndex ib = AssignArrayIndexForID(b);

    if (a == b)
    {
        // it's implicitly implied that sector is visible from itself
        return;
    }

    CheckSize(ia, a);
    CheckSize(ib, b);

    pvs[ia].insert(ib);
    pvs[ib].insert(ia);
}

void RTGL1::SectorVisibility::Reset()
{
    pvs.clear();
    pvs.clear();

    lastSectorArrayIndex = SECTOR_ARRAY_INDEX_BASE_VALUE;
    sectorIDToArrayIndex.clear();

    memset(sectorArrayIndexToID, 0, sizeof(sectorArrayIndexToID));

    // but always keep potential visibility for sector ID = 0.
    
    SectorID defaultSectorId = { 0 };
    SetPotentialVisibility(defaultSectorId, defaultSectorId);
}

bool RTGL1::SectorVisibility::ArePotentiallyVisibleSectorsExist(SectorArrayIndex forThisSector) const
{
    return pvs.find(forThisSector) != pvs.end();
}

const rgl::unordered_set<RTGL1::SectorArrayIndex> &RTGL1::SectorVisibility::GetPotentiallyVisibleSectors(SectorArrayIndex fromThisSector)
{
    // should exist
    assert(ArePotentiallyVisibleSectorsExist(fromThisSector));
    
    return pvs[fromThisSector];
}

void RTGL1::SectorVisibility::CheckSize(SectorArrayIndex index, SectorID id) const
{
    assert(SectorIDToArrayIndex(id) == index);
    const auto &sv = pvs.find(index);

    if (sv != pvs.end() && sv->second.size() >= RTGL1::MAX_SECTOR_COUNT)
    {
        throw RTGL1::RgException(
            RG_TOO_MANY_SECTORS,
            "Number of potentially visible sectors for the sector #" + std::to_string(id.GetID()) +
            " exceeds the limit of " + std::to_string(RTGL1::MAX_SECTOR_COUNT));
    }
}

RTGL1::SectorArrayIndex RTGL1::SectorVisibility::AssignArrayIndexForID(SectorID id)
{
    // if doesn't exist
    if (sectorIDToArrayIndex.find(id) == sectorIDToArrayIndex.end())
    {
        assert(lastSectorArrayIndex < MAX_SECTOR_COUNT);


        // add new
        sectorIDToArrayIndex[id] = SectorArrayIndex{ lastSectorArrayIndex };
        sectorArrayIndexToID[lastSectorArrayIndex] = id;
            

        lastSectorArrayIndex++;
    }

    return sectorIDToArrayIndex[id];
}

RTGL1::SectorArrayIndex RTGL1::SectorVisibility::SectorIDToArrayIndex(SectorID id) const
{
    const auto &found = sectorIDToArrayIndex.find(id);

    if (found == sectorIDToArrayIndex.end())
    {
        throw RgException(RG_ERROR_INCORRECT_SECTOR,
                          "Can't find sector ID=" + std::to_string(id.GetID()) + 
                          ". Probably, it wasn't referenced with rgSetPotentialVisibility");
    }

    return found->second;
}

RTGL1::SectorID RTGL1::SectorVisibility::SectorArrayIndexToID(SectorArrayIndex index) const
{
    const SectorID &id = sectorArrayIndexToID[index.GetArrayIndex()];

    assert(sectorIDToArrayIndex.find(id) != sectorIDToArrayIndex.end());
    return id;
}

