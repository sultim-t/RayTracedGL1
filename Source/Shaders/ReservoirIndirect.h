// Copyright (c) 2022 Sultim Tsyrendashiev
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

#ifndef RESERVOIR_INDIRECT_H_
#define RESERVOIR_INDIRECT_H_

struct SampleIndirect
{
    vec3 position;
    vec3 normal;
    vec3 radiance;
};

struct ReservoirIndirect
{
    SampleIndirect  selected;
    float           selected_targetPdf; // todo: remove? as it's easily calculated
    float           weightSum;
    uint            M;
};



SampleIndirect emptySampleIndirect()
{
    SampleIndirect s;
    s.position = vec3(0.0);
    s.normal = vec3(0.0);
    s.radiance = vec3(0.0);
    return s;
}

ReservoirIndirect emptyReservoirIndirect()
{
    ReservoirIndirect r;
    r.selected.position = vec3(0.0);
    r.selected.normal = vec3(0.0);
    r.selected.radiance = vec3(0.0);
    r.selected_targetPdf = 0.0;
    r.weightSum = 0.0;
    r.M = 0;
    return r;
}

float calcSelectedSampleWeightIndirect(const ReservoirIndirect r)
{
    return safePositiveRcp(r.selected_targetPdf) * (r.weightSum / float(max(1, r.M)));
}

void normalizeReservoirIndirect(inout ReservoirIndirect r, uint maxM)
{
    r.weightSum /= float(max(r.M, 1));

    r.M = clamp(r.M, 0, maxM);
    r.weightSum *= r.M;
}

void updateReservoirIndirect(
    inout ReservoirIndirect r,
    const SampleIndirect newSample, float targetPdf, float oneOverSourcePdf, 
    float rnd)
{
    float weight = targetPdf * oneOverSourcePdf;

    r.weightSum += weight;
    r.M += 1;

    if (rnd * r.weightSum < weight)
    {
        r.selected = newSample;
        r.selected_targetPdf = targetPdf;
    }
}

void initCombinedReservoirIndirect(out ReservoirIndirect combined, const ReservoirIndirect base)
{
    combined.selected = base.selected;
    combined.selected_targetPdf = base.selected_targetPdf;
    combined.weightSum = base.weightSum;
    combined.M = base.M;
}

bool updateCombinedReservoirIndirect(inout ReservoirIndirect combined, const ReservoirIndirect b, float rnd)
{
    float weight = b.weightSum;

    combined.weightSum += weight;
    combined.M += b.M;
    if (rnd * combined.weightSum < weight)
    {
        combined.selected = b.selected;
        combined.selected_targetPdf = b.selected_targetPdf;

        return true;
    }

    return false;
}

void updateCombinedReservoirIndirect_newSurf(inout ReservoirIndirect combined, const ReservoirIndirect b, float targetPdf_b, float rnd)
{
    // targetPdf_b is targetPdf(b.selected) for pixel q
    // but b.selected_targetPdf was calculated for pixel q'
    // so need to renormalize weight
    float weight = targetPdf_b * safePositiveRcp(b.selected_targetPdf) * b.weightSum;

    combined.weightSum += weight;
    combined.M += b.M;
    if (rnd * combined.weightSum < weight)
    {
        combined.selected = b.selected;
        combined.selected_targetPdf = targetPdf_b;
    }
}



#ifdef DESC_SET_GLOBAL_UNIFORM
#ifdef DESC_SET_RESTIR_INDIRECT
bool rgi_TryGetPixOffset(const ivec2 pix, out uint offset)
{
    offset = pix.y * uint(globalUniform.renderWidth) + pix.x;
    return offset >= 0 && offset < uint(globalUniform.renderWidth) * uint(globalUniform.renderHeight);
}

void restirIndirect_StoreInitialSample(const ivec2 pix, const SampleIndirect s)
{
    uint offset;
    if (!rgi_TryGetPixOffset(pix, offset))
    {
        return;
    }

    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 0] = floatBitsToUint(s.position.x);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 1] = floatBitsToUint(s.position.y);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 2] = floatBitsToUint(s.position.z);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 3] = encodeNormal(s.normal);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 4] = encodeE5B9G9R9(s.radiance);

#if PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS != 5
    #error "Size mismatch"
#endif
}

void restirIndirect_StoreReservoir(const ivec2 pix, ReservoirIndirect r)
{
    uint offset;
    if (!rgi_TryGetPixOffset(pix, offset))
    {
        return;
    }
    
    if (!isinf(r.weightSum) && !isnan(r.weightSum) && r.weightSum >= 0.0)
    {
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 0] = floatBitsToUint(r.selected.position.x);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 1] = floatBitsToUint(r.selected.position.y);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 2] = floatBitsToUint(r.selected.position.z);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 3] = encodeNormal(r.selected.normal);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 4] = encodeE5B9G9R9(r.selected.radiance);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 5] = floatBitsToUint(r.selected_targetPdf);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 6] = floatBitsToUint(r.weightSum);
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 7] = r.M;
    }
    else
    {
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 0] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 1] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 2] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 3] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 4] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 5] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 6] = 0;
        g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 7] = 0;
    }

#if PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS != 8
    #error "Size mismatch"
#endif
}

SampleIndirect restirIndirect_LoadInitialSample(const ivec2 pix)
{
    SampleIndirect s;

    uint offset;
    if (!rgi_TryGetPixOffset(pix, offset))
    {
        s = emptySampleIndirect();
        return s;
    }

    s.position.x = uintBitsToFloat(g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 0]);
    s.position.y = uintBitsToFloat(g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 1]);
    s.position.z = uintBitsToFloat(g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 2]);
    s.normal     =    decodeNormal(g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 3]);
    s.radiance   =  decodeE5B9G9R9(g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 4]);

    return s;
}

#define INDIR_LOAD_RESERVOIR_T(BUFFER_T) \
    r.selected.position.x   = uintBitsToFloat(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 0]); \
    r.selected.position.y   = uintBitsToFloat(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 1]); \
    r.selected.position.z   = uintBitsToFloat(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 2]); \
    r.selected.normal       =    decodeNormal(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 3]); \
    r.selected.radiance     =  decodeE5B9G9R9(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 4]); \
    r.selected_targetPdf    = uintBitsToFloat(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 5]); \
    r.weightSum             = uintBitsToFloat(BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 6]); \
    r.M                     =                (BUFFER_T[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 7]);

ReservoirIndirect restirIndirect_LoadReservoir(const ivec2 pix)
{
    ReservoirIndirect r;

    uint offset;
    if (!rgi_TryGetPixOffset(pix, offset))
    {
        r = emptyReservoirIndirect();
        return r;
    }

    INDIR_LOAD_RESERVOIR_T(g_restirIndirectReservoirs);
    return r;
}

ReservoirIndirect restirIndirect_LoadReservoir_Prev(const ivec2 pix)
{
    ReservoirIndirect r;

    uint offset;
    if (!rgi_TryGetPixOffset(pix, offset))
    {
        r = emptyReservoirIndirect();
        return r;
    }

    INDIR_LOAD_RESERVOIR_T(g_restirIndirectReservoirs_Prev);
    return r;
}

#endif // DESC_SET_RESTIR_INDIRECT
#endif // DESC_SET_GLOBAL_UNIFORM

#endif // RESERVOIR_INDIRECT_H_