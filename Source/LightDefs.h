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

#include <functional>

namespace RTGL1
{


constexpr size_t MAX_SECTOR_COUNT = 1024;
constexpr size_t MAX_LIGHT_LIST_SIZE = 1024;


// Passed to the library by user
typedef uint64_t UniqueLightID;


// Index in the global light array.
// Used to match lights by UniqueLightID between current and previous frames,
// as indices for the same light in them can be different, and only UniqueLightID is constant.
struct LightArrayIndex
{
    typedef uint32_t index_t;
    index_t _indexInGlobalArray;

    index_t GetArrayIndex() const
    {
        return _indexInGlobalArray;
    }
    bool operator==(const LightArrayIndex &other) const
    {
        return _indexInGlobalArray == other._indexInGlobalArray;
    }
};
static_assert(std::is_pod_v<LightArrayIndex>, "");


struct SectorID
{
    typedef uint32_t id_t;
    id_t _id;

    id_t GetID() const
    {
        return _id;
    }
    bool operator==(const SectorID &other) const
    {
        return _id == other._id;
    }
};
static_assert(std::is_pod_v<SectorID>, "");


}


// default hash specialializations

template<>
struct std::hash<RTGL1::LightArrayIndex>
{
    std::size_t operator()(RTGL1::LightArrayIndex const &s) const noexcept
    {
        return std::hash<RTGL1::LightArrayIndex::index_t>{}(s._indexInGlobalArray);
    }
};

template<>
struct std::hash<RTGL1::SectorID>
{
    std::size_t operator()(RTGL1::SectorID const &s) const noexcept
    {
        return std::hash<RTGL1::SectorID::id_t>{}(s._id);
    }
};