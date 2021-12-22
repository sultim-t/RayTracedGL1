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

#include <unordered_set>
#include <unordered_map>

#include "LightDefs.h"

namespace RTGL1
{

class SectorVisibility
{
public:
    SectorVisibility() = default;
    ~SectorVisibility() = default;

    SectorVisibility(const SectorVisibility &other) = delete;
    SectorVisibility(SectorVisibility &&other) noexcept = delete;
    SectorVisibility & operator=(const SectorVisibility &other) = delete;
    SectorVisibility & operator=(SectorVisibility &&other) noexcept = delete;

    // Potential visibility is a commutative relation.
    void SetPotentialVisibility(SectorID a, SectorID b);
    void Reset();

    const std::unordered_set<SectorID> &GetPotentiallyVisibleSectors(SectorID fromThisSector);

private:
    void CheckSize(SectorID i) const;

public:
    std::unordered_map<SectorID, std::unordered_set<SectorID>> pvs;
};

}