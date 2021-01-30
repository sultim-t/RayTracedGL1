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

#include "VertexCollectorFilterType.h"

#include <cassert>

// file scope typedefs
typedef VertexCollectorFilterTypeFlagBits FT;
typedef VertexCollectorFilterTypeFlags FL;

uint32_t VertexCollectorFilterTypeFlagsToOffset(VertexCollectorFilterTypeFlags flags)
{
    uint32_t result = 0;

    for (auto cf : VertexCollectorFilterGroup_ChangeFrequency)
    {
        for (auto pt : VertexCollectorFilterGroup_PassThrough)
        {
            if ((cf | pt) == flags)
            {
                return result;
            }

            result++;
        }
    }

    return UINT32_MAX;
}

struct FLName
{
    FL          flags;
    const char  *name;
};

const static FLName FL_NAMES[] =
{
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_OPAQUE,                "BLAS static opaque"                    },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_ALPHA_TESTED,          "BLAS static alpha tested"              },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_BLEND_ADDITIVE,        "BLAS static blended additive"          },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_BLEND_UNDER,           "BLAS static blended under"             },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_REFRACTIVE_REFLECTIVE, "BLAS static refractive-reflective"     },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_ONLY_REFLECTIVE,       "BLAS static reflective"                },
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_PORTAL,                "BLAS static portal"                    },

    { FT::CF_STATIC_MOVABLE     | FT::PT_OPAQUE,                "BLAS movable opaque"                   },
    { FT::CF_STATIC_MOVABLE     | FT::PT_ALPHA_TESTED,          "BLAS movable alpha tested"             },
    { FT::CF_STATIC_MOVABLE     | FT::PT_BLEND_ADDITIVE,        "BLAS movable blended additive"         },
    { FT::CF_STATIC_MOVABLE     | FT::PT_BLEND_UNDER,           "BLAS movable blended under"            },
    { FT::CF_STATIC_MOVABLE     | FT::PT_REFRACTIVE_REFLECTIVE, "BLAS movable refractive-reflective"    },
    { FT::CF_STATIC_MOVABLE     | FT::PT_ONLY_REFLECTIVE,       "BLAS movable reflective"               },
    { FT::CF_STATIC_MOVABLE     | FT::PT_PORTAL,                "BLAS movable portal"                   },

    { FT::CF_DYNAMIC            | FT::PT_OPAQUE,                "BLAS dynamic opaque"                   },
    { FT::CF_DYNAMIC            | FT::PT_ALPHA_TESTED,          "BLAS dynamic alpha tested"             },
    { FT::CF_DYNAMIC            | FT::PT_BLEND_ADDITIVE,        "BLAS dynamic blended additive"         },
    { FT::CF_DYNAMIC            | FT::PT_BLEND_UNDER,           "BLAS dynamic blended under"            },
    { FT::CF_DYNAMIC            | FT::PT_REFRACTIVE_REFLECTIVE, "BLAS dynamic refractive-reflective"    },
    { FT::CF_DYNAMIC            | FT::PT_ONLY_REFLECTIVE,       "BLAS dynamic reflective"               },
    { FT::CF_DYNAMIC            | FT::PT_PORTAL,                "BLAS dynamic portal"                   },
};

const char *GetVertexCollectorFilterTypeFlagsNameForBLAS(VertexCollectorFilterTypeFlags flags)
{
    for (const FLName &p : FL_NAMES)
    {
        if (p.flags == flags)
        {
            return p.name;
        }
    }

    // in debug mode, every BLAS must have a name
    assert(0);
    return nullptr;
}

VertexCollectorFilterTypeFlags GetVertexCollectorFilterTypeFlagsForGeometry(const RgGeometryUploadInfo &info)
{
    FL flags = 0;

    switch (info.geomType)
    {
        case RG_GEOMETRY_TYPE_STATIC:
        {
            flags |= (FL)FT::CF_STATIC_NON_MOVABLE;
            break;
        }
        case RG_GEOMETRY_TYPE_STATIC_MOVABLE:
        {
            flags |= (FL)FT::CF_STATIC_MOVABLE;
            break;
        }
        case RG_GEOMETRY_TYPE_DYNAMIC:
        {
            flags |= (FL)FT::CF_DYNAMIC;
            break;
        }
        default: assert(0);
    }

    switch (info.passThroughType)
    {
        case RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE:
        {
            flags |= (FL)FT::PT_OPAQUE;
            break;
        }
        case RG_GEOMETRY_PASS_THROUGH_TYPE_ALPHA_TESTED:
        {
            flags |= (FL)FT::PT_ALPHA_TESTED;
            break;
        }
        case RG_GEOMETRY_PASS_THROUGH_TYPE_BLEND_ADDITIVE:
        {
            flags |= (FL)FT::PT_BLEND_ADDITIVE;
            break;
        }
        case RG_GEOMETRY_PASS_THROUGH_TYPE_BLEND_UNDER:
        {
            flags |= (FL)FT::PT_BLEND_UNDER;
            break;
        }
        default: assert(0);
    }

    return flags;
}
