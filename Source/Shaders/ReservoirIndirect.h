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
    float           selected_targetPdf;
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

#ifdef DESC_SET_GLOBAL_UNIFORM
#ifdef DESC_SET_RESTIR_INDIRECT
uint getPixOffset(const ivec2 pix)
{
    return pix.y * int(globalUniform.renderWidth) + pix.x;
}
void restirIndirect_StoreInitialSample(const ivec2 pix, const SampleIndirect s)
{
    uint offset = getPixOffset(pix);

    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 0] = floatBitsToUint(s.position.x);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 1] = floatBitsToUint(s.position.y);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 2] = floatBitsToUint(s.position.z);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 3] = encodeNormal(s.normal);
    g_restirIndirectInitialSamples[offset * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS + 4] = encodeE5B9G9R9(s.radiance);

#if PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS != 5
    #error "Size mismatch"
#endif
}

void restirIndirect_StoreReservoir(const ivec2 pix, const ReservoirIndirect r)
{
    uint offset = getPixOffset(pix);

    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 0] = floatBitsToUint(r.selected.position.x);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 1] = floatBitsToUint(r.selected.position.y);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 2] = floatBitsToUint(r.selected.position.z);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 3] = encodeNormal(r.selected.normal);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 4] = encodeE5B9G9R9(r.selected.radiance);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 5] = floatBitsToUint(r.selected_targetPdf);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 6] = floatBitsToUint(r.weightSum);
    g_restirIndirectReservoirs[offset * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS + 7] = r.M;

#if PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS != 8
    #error "Size mismatch"
#endif
}

SampleIndirect restirIndirect_LoadInitialSample(const ivec2 pix)
{
    uint offset = getPixOffset(pix);

    SampleIndirect s;

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
    uint offset = getPixOffset(pix);
    ReservoirIndirect r;
    INDIR_LOAD_RESERVOIR_T(g_restirIndirectReservoirs);
    return r;
}

ReservoirIndirect restirIndirect_LoadReservoir_Prev(const ivec2 pix)
{
    uint offset = getPixOffset(pix);
    ReservoirIndirect r;
    INDIR_LOAD_RESERVOIR_T(g_restirIndirectReservoirs_Prev);
    return r;
}

#endif // DESC_SET_RESTIR_INDIRECT
#endif // DESC_SET_GLOBAL_UNIFORM

#endif // RESERVOIR_INDIRECT_H_