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

#include "Common.h"
#include "RTGL1/RTGL1.h"

enum class VertexCollectorFilterTypeFlagBits : uint32_t
{
    NONE                        = 0,

    CF_STATIC_NON_MOVABLE       = 1 << 0,
    CF_STATIC_MOVABLE           = 2 << 0,
    CF_DYNAMIC                  = 4 << 0,
    MASK_CHANGE_FREQUENCY_GROUP = CF_STATIC_NON_MOVABLE | CF_STATIC_MOVABLE | CF_DYNAMIC,

    PT_OPAQUE                   = 1 << 3,
    PT_ALPHA_TESTED             = 2 << 3,
    PT_TRANSPARENT_BLENDED      = 4 << 3,
    PT_REFRACTIVE_REFLECTIVE    = 8 << 3,
    PT_ONLY_REFLECTIVE          = 16 << 3,
    PT_PORTAL                   = 32 << 3,
    MASK_PASS_THROUGH_GROUP     = PT_OPAQUE | PT_ALPHA_TESTED | PT_TRANSPARENT_BLENDED | PT_REFRACTIVE_REFLECTIVE | PT_ONLY_REFLECTIVE | PT_PORTAL,
};
typedef uint32_t VertexCollectorFilterTypeFlags;


constexpr VertexCollectorFilterTypeFlagBits VertexCollectorFilterGroup_ChangeFrequency[] =
{
    VertexCollectorFilterTypeFlagBits::CF_STATIC_NON_MOVABLE,
    VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE,
    VertexCollectorFilterTypeFlagBits::CF_DYNAMIC,
};

constexpr VertexCollectorFilterTypeFlagBits VertexCollectorFilterGroup_PassThrough[] =
{
    VertexCollectorFilterTypeFlagBits::PT_OPAQUE,
    VertexCollectorFilterTypeFlagBits::PT_ALPHA_TESTED,
    VertexCollectorFilterTypeFlagBits::PT_TRANSPARENT_BLENDED,
    VertexCollectorFilterTypeFlagBits::PT_REFRACTIVE_REFLECTIVE,
    VertexCollectorFilterTypeFlagBits::PT_ONLY_REFLECTIVE,
    VertexCollectorFilterTypeFlagBits::PT_PORTAL,
};

inline VertexCollectorFilterTypeFlags operator|(VertexCollectorFilterTypeFlagBits a, VertexCollectorFilterTypeFlagBits b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(static_cast<FL>(a) | static_cast<FL>(b));
}

inline VertexCollectorFilterTypeFlags operator|(VertexCollectorFilterTypeFlags a, VertexCollectorFilterTypeFlagBits b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(a | static_cast<FL>(b));
}

inline VertexCollectorFilterTypeFlags operator|(VertexCollectorFilterTypeFlagBits a, VertexCollectorFilterTypeFlags b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(static_cast<FL>(a) | b);
}

inline VertexCollectorFilterTypeFlags operator&(VertexCollectorFilterTypeFlagBits a, VertexCollectorFilterTypeFlagBits b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(static_cast<FL>(a) & static_cast<FL>(b));
}

inline VertexCollectorFilterTypeFlags operator&(VertexCollectorFilterTypeFlags a, VertexCollectorFilterTypeFlagBits b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(a & static_cast<FL>(b));
}

inline VertexCollectorFilterTypeFlags operator&(VertexCollectorFilterTypeFlagBits a, VertexCollectorFilterTypeFlags b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(static_cast<FL>(a) & b);
}

uint32_t VertexCollectorFilterTypeFlagsToOffset(VertexCollectorFilterTypeFlags flags);
const char *GetVertexCollectorFilterTypeFlagsNameForBLAS(VertexCollectorFilterTypeFlags flags);
VertexCollectorFilterTypeFlags GetVertexCollectorFilterTypeFlagsForGeometry(const RgGeometryUploadInfo &info);
