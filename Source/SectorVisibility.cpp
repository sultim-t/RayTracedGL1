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

void RTGL1::SectorVisibility::SetPotentialVisibility(SectorID a, SectorID b)
{
    if (a == b)
    {
        // it's implicitly implied that sector is visible from itself
        return;
    }

    CheckSize(a);
    CheckSize(b);

    pvs[a].insert(b);
    pvs[b].insert(a);
}

void RTGL1::SectorVisibility::Reset()
{
    pvs.clear();
    pvs.clear();
}

bool RTGL1::SectorVisibility::ArePotentiallyVisibleSectorsExist(SectorID forThisSector) const
{
    return pvs.find(forThisSector) != pvs.end();
}

const std::unordered_set<RTGL1::SectorID> &RTGL1::SectorVisibility::GetPotentiallyVisibleSectors(SectorID fromThisSector)
{
    // should exist
    assert(ArePotentiallyVisibleSectorsExist(fromThisSector));
    
    return pvs[fromThisSector];
}

void RTGL1::SectorVisibility::CheckSize(SectorID i) const
{
    using namespace std::string_literals;

    const auto &s = pvs.find(i);
    if (s == pvs.end())
    {
        return;
    }

    if (s->second.size() >= RTGL1::MAX_SECTOR_COUNT)
    {
        throw RTGL1::RgException(
            RG_TOO_MANY_SECTORS,
            "Number of potentially visible sectors for the sector #"s + std::to_string(i.GetID()) +
            " exceeds the limit of " + std::to_string(RTGL1::MAX_SECTOR_COUNT));
    }
}
