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

#ifndef RESERVOIR_H_
#define RESERVOIR_H_

struct Reservoir
{
    uint    selected;
    float   selected_targetPdf;
    float   weightSum;
    uint    M;
};

Reservoir emptyReservoir()
{
    Reservoir r;
    r.selected = LIGHT_INDEX_NONE;
    r.selected_targetPdf = 0.0;
    r.weightSum = 0.0;
    r.M = 0;
    return r;
}

bool isReservoirValid(const Reservoir r)
{
    return r.selected != LIGHT_INDEX_NONE;
}

float safePositiveRcp(float f)
{
    return f <= 0.0 ? 0.0 : 1.0 / f;
}

float calcSelectedSampleWeight(const Reservoir r)
{
    return safePositiveRcp(r.selected_targetPdf) * (r.weightSum / float(max(1, r.M)));
}

void normalizeReservoir(inout Reservoir r, uint maxM)
{
    r.weightSum /= float(max(r.M, 1));

    r.M = clamp(r.M, 0, maxM);
    r.weightSum *= r.M;
}

void updateReservoir(
    inout Reservoir r, 
    uint lightIndex, float targetPdf, 
    float oneOverSourcePdf, float rnd)
{
    float weight = targetPdf * oneOverSourcePdf;

    r.weightSum += weight;
    r.M += 1;
    
    if (rnd * r.weightSum < weight)
    {
        r.selected = lightIndex;
        r.selected_targetPdf = targetPdf;
    }
}

void initCombinedReservoir(out Reservoir combined, const Reservoir base)
{
    combined.selected = base.selected;
    combined.selected_targetPdf = base.selected_targetPdf;
    combined.weightSum = base.weightSum;
    combined.M = base.M;
}

void updateCombinedReservoir(inout Reservoir combined, const Reservoir b, float rnd)
{
    float weight = b.weightSum;

    combined.weightSum += weight;
    combined.M += b.M;
    if (rnd * combined.weightSum < weight)
    {
        combined.selected = b.selected;
        combined.selected_targetPdf = b.selected_targetPdf;
    }
}

void updateCombinedReservoir_newSurf(inout Reservoir combined, const Reservoir b, float targetPdf_b, float rnd)
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


#ifdef DESC_SET_FRAMEBUFFERS
uvec4 packReservoir(const Reservoir r)
{
    if (!isinf(r.weightSum) && !isnan(r.weightSum))
    {        
        return uvec4(
            (min(r.M, 65535u) << 16u) | min(r.selected, 65535u),
            floatBitsToUint(r.selected_targetPdf),
            floatBitsToUint(r.weightSum),
            0
        );
    }
    else
    {
        return uvec4(
            min(LIGHT_INDEX_NONE, 65535u),
            floatBitsToUint(0.0),
            floatBitsToUint(0.0),
            0
        );
    }
}

Reservoir unpackReservoir(const uvec4 p)
{
    Reservoir r;
    r.selected              = (p[0]       ) & 65535u;
    r.M                     = (p[0] >> 16u) & 65535u;
    r.selected_targetPdf    = uintBitsToFloat(p[1]);
    r.weightSum             = uintBitsToFloat(p[2]);
    return r;
}

void imageStoreReservoir(const Reservoir r, const ivec2 pix)
{
    imageStore(framebufReservoirs, pix, packReservoir(r));
}

// "Rearchitecting spatiotemporal resampling for production" C. Wyman, Alexey Panteleev
// To avoid a mid-frame global barrier, use previous frame reservoirs for reading
Reservoir imageLoadReservoir_Prev(const ivec2 pix)
{
    return unpackReservoir(imageLoad(framebufReservoirs_Prev, pix));
}

void imageStoreReservoirInitial(const Reservoir normalized, const ivec2 pix)
{
    imageStore(framebufReservoirsInitial, pix, packReservoir(normalized));
}

Reservoir imageLoadReservoirInitial(const ivec2 pix)
{
    return unpackReservoir(imageLoad(framebufReservoirsInitial, pix));
}
#endif // DESC_SET_FRAMEBUFFERS

#endif // RESERVOIR_H_