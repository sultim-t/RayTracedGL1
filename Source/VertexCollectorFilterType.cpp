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
#include <cstring>

#include "Const.h"
#include "Generated/ShaderCommonC.h"

static_assert(
    sizeof(RTGL1::VertexCollectorFilterGroup_ChangeFrequency)   / sizeof(RTGL1::VertexCollectorFilterGroup_ChangeFrequency[0]) * 
    sizeof(RTGL1::VertexCollectorFilterGroup_PassThrough)       / sizeof(RTGL1::VertexCollectorFilterGroup_PassThrough[0]) *
    sizeof(RTGL1::VertexCollectorFilterGroup_PrimaryVisibility) / sizeof(RTGL1::VertexCollectorFilterGroup_PrimaryVisibility[0])
    == MAX_TOP_LEVEL_INSTANCE_COUNT, "It's recommended for MAX_TOP_LEVEL_INSTANCE_COUNT to be such value");

typedef uint8_t FlagToIndexType;
// 8 bits per byte
constexpr uint32_t FlagToIndexTypeMaxValue = 1 << (8 * sizeof(FlagToIndexType));

static_assert(MAX_TOP_LEVEL_INSTANCE_COUNT < FlagToIndexTypeMaxValue, "");

// file scope typedefs
typedef RTGL1::VertexCollectorFilterTypeFlagBits FT;
typedef RTGL1::VertexCollectorFilterTypeFlags FL;

void RTGL1::VertexCollectorFilterTypeFlags_IterateOverFlags(std::function<void(FL)> f)
{
    for (auto cf : VertexCollectorFilterGroup_ChangeFrequency)
    {
        for (auto pt : VertexCollectorFilterGroup_PassThrough)
        {
            for (auto pm : VertexCollectorFilterGroup_PrimaryVisibility)
            {
                f(cf | pt | pm);
            }
        }
    }
}

// max flag value in a group
constexpr uint32_t MAX_FLAG_VALUE_CF = 4;
constexpr uint32_t MAX_FLAG_VALUE_PT = 4;
constexpr uint32_t MAX_FLAG_VALUE_PV = 16;

static FlagToIndexType FlagToIndex[MAX_FLAG_VALUE_CF][MAX_FLAG_VALUE_PT][MAX_FLAG_VALUE_PV];

static uint32_t AllBottomLevelGeomsCount = 0;
static uint32_t OffsetInGlobalArray[MAX_FLAG_VALUE_CF][MAX_FLAG_VALUE_PT][MAX_FLAG_VALUE_PV];
static uint32_t AmountInGlobalArray[MAX_FLAG_VALUE_CF][MAX_FLAG_VALUE_PT][MAX_FLAG_VALUE_PV];

void RTGL1::VertexCollectorFilterTypeFlags_Init()
{
    memset(FlagToIndex, 0xFF, sizeof(FlagToIndex));

    AllBottomLevelGeomsCount = 0;
    memset(OffsetInGlobalArray, 0, sizeof(OffsetInGlobalArray));


    uint32_t index = 0;

    for (auto flcf : VertexCollectorFilterGroup_ChangeFrequency)
    {
        for (auto flpt : VertexCollectorFilterGroup_PassThrough)
        {
            for (auto flpv : VertexCollectorFilterGroup_PrimaryVisibility)
            {
                uint32_t cf = (uint32_t)flcf >> VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_CF;
                uint32_t pt = (uint32_t)flpt >> VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_PT;
                uint32_t pv = (uint32_t)flpv >> VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_PV;

                assert(cf > 0 && cf <= MAX_FLAG_VALUE_CF);
                assert(pt > 0 && pt <= MAX_FLAG_VALUE_PT);
                assert(pv > 0 && pv <= MAX_FLAG_VALUE_PV);

                assert(index < MAX_TOP_LEVEL_INSTANCE_COUNT);
                assert(index < FlagToIndexTypeMaxValue);

                // flags bits start with 1, not 0
                cf--;
                pt--;
                pv--;

                FlagToIndex[cf][pt][pv] = (FlagToIndexType)index;

                index++;


                // amount of world objects is significantly larger than first-person ones
                bool hasLowerAmount = 
                    (flpv & VertexCollectorFilterTypeFlagBits::PV_FIRST_PERSON) || 
                    (flpv & VertexCollectorFilterTypeFlagBits::PV_FIRST_PERSON_VIEWER);

                assert(LOWER_BOTTOM_LEVEL_GEOMETRIES_COUNT < MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT);
                
                uint32_t count = hasLowerAmount ? LOWER_BOTTOM_LEVEL_GEOMETRIES_COUNT : MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT;

                AmountInGlobalArray[cf][pt][pv] = count;
                OffsetInGlobalArray[cf][pt][pv] = AllBottomLevelGeomsCount;
                AllBottomLevelGeomsCount += count;
            }
        }
    }
}

static void GetIndices(RTGL1::VertexCollectorFilterTypeFlags flags, uint32_t &cf, uint32_t &pt, uint32_t &pv)
{
    cf = (flags & FT::MASK_CHANGE_FREQUENCY_GROUP  ) >> RTGL1::VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_CF;
    pt = (flags & FT::MASK_PASS_THROUGH_GROUP      ) >> RTGL1::VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_PT;
    pv = (flags & FT::MASK_PRIMARY_VISIBILITY_GROUP) >> RTGL1::VERTEX_COLLECTOR_FILTER_TYPE_BIT_OFFSET_PV;

    assert(cf > 0 && cf <= MAX_FLAG_VALUE_CF);
    assert(pt > 0 && pt <= MAX_FLAG_VALUE_PT);
    assert(pv > 0 && pv <= MAX_FLAG_VALUE_PV);

    // flags bits start with 1, not 0
    cf--;
    pt--;
    pv--;

    assert(FlagToIndex[cf][pt][pv] < MAX_TOP_LEVEL_INSTANCE_COUNT);
}

uint32_t RTGL1::VertexCollectorFilterTypeFlags_GetID(VertexCollectorFilterTypeFlags flags)
{
    uint32_t cf, pt, pv;
    GetIndices(flags, cf, pt, pv);

    return FlagToIndex[cf][pt][pv];
}

uint32_t RTGL1::VertexCollectorFilterTypeFlags_GetAllBottomLevelGeomsCount()
{
    return AllBottomLevelGeomsCount;
}

uint32_t RTGL1::VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray(VertexCollectorFilterTypeFlags flags)
{
    uint32_t cf, pt, pv;
    GetIndices(flags, cf, pt, pv);

    return OffsetInGlobalArray[cf][pt][pv];
}

uint32_t RTGL1::VertexCollectorFilterTypeFlags_GetAmountInGlobalArray(VertexCollectorFilterTypeFlags flags)
{
    uint32_t cf, pt, pv;
    GetIndices(flags, cf, pt, pv);

    return AmountInGlobalArray[cf][pt][pv];
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
    { FT::CF_STATIC_NON_MOVABLE | FT::PT_REFRACT,               "BLAS static refract"                   },

    { FT::CF_STATIC_MOVABLE     | FT::PT_OPAQUE,                "BLAS movable opaque"                   },
    { FT::CF_STATIC_MOVABLE     | FT::PT_ALPHA_TESTED,          "BLAS movable alpha tested"             },
    { FT::CF_STATIC_MOVABLE     | FT::PT_REFRACT,               "BLAS movable refract"                  },

    { FT::CF_DYNAMIC            | FT::PT_OPAQUE,                "BLAS dynamic opaque"                   },
    { FT::CF_DYNAMIC            | FT::PT_ALPHA_TESTED,          "BLAS dynamic alpha tested"             },
    { FT::CF_DYNAMIC            | FT::PT_REFRACT,               "BLAS dynamic refract"                  },
};

const char *RTGL1::VertexCollectorFilterTypeFlags_GetNameForBLAS(FL flags)
{
    for (const FLName &p : FL_NAMES)
    {
        if ((p.flags & flags) == p.flags)
        {
            return p.name;
        }
    }

    // in debug mode, every BLAS must have a name
    assert(0);
    return nullptr;
}

FL RTGL1::VertexCollectorFilterTypeFlags_GetForGeometry(const RgGeometryUploadInfo &info)
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
        case RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_ONLY_REFLECT:
        case RG_GEOMETRY_PASS_THROUGH_TYPE_MIRROR:
        case RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL:
        {
            flags |= ( FL )FT::PT_OPAQUE;
            break;
        }
        case RG_GEOMETRY_PASS_THROUGH_TYPE_ALPHA_TESTED:
        {
            flags |= ( FL )FT::PT_ALPHA_TESTED;
            break;
        }
        case RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_REFLECT_REFRACT:
        case RG_GEOMETRY_PASS_THROUGH_TYPE_GLASS_REFLECT_REFRACT:
        {
            flags |= ( FL )FT::PT_REFRACT;
            break;
        }
        default: assert(0);
    }

    switch (info.visibilityType)
    {
        case RG_GEOMETRY_VISIBILITY_TYPE_WORLD_0:
        {
            flags |= (FL)FT::PV_WORLD_0;
            break;
        }
        case RG_GEOMETRY_VISIBILITY_TYPE_WORLD_1:
        {
            flags |= (FL)FT::PV_WORLD_1;
            break;
        }
        case RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2:
        {
            flags |= (FL)FT::PV_WORLD_2;
            break;
        }
        case RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON:
        {
            flags |= (FL)FT::PV_FIRST_PERSON;
            break;
        }
        case RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER:
        {
            flags |= (FL)FT::PV_FIRST_PERSON_VIEWER;
            break;
        }
    #if RAYCULLMASK_SKY_IS_WORLD2
        case RG_GEOMETRY_VISIBILITY_TYPE_SKY:
        {
            flags |= (FL)FT::PV_WORLD_2;
            break;
        }
    #else
        #error Handle RG_DRAW_FRAME_RAY_CULL_SKY_BIT, if there is no WORLD_2
    #endif
        default: assert(0);
    }

    return flags;
}
