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


#define RESERVOIR_W_MAX (1.0 / 0.0001)


struct Reservoir
{
    uint selected;
    float selected_targetPdf; /* TODO: store/load; used for grid ONLY for now */
    float weightSum;
    float M;
    float W;
};

Reservoir emptyReservoir()
{
    Reservoir r;
    r.selected = LIGHT_INDEX_NONE;
    r.selected_targetPdf = 0.0;
    r.weightSum = 0.0;
    r.M = 0.0;
    r.W = 0.0;
    return r;
}

bool isReservoirValid(const Reservoir r)
{
    return 
        r.selected != LIGHT_INDEX_NONE && 
        r.W > 0.0 && !isnan(r.W) && !isinf(r.W);
}

void calcReservoirW(inout Reservoir r, float targetPdf_selected)
{
    if (targetPdf_selected > 0.00001 && r.M > 0)
    {
        r.W = (1.0 / targetPdf_selected) * (r.weightSum / r.M);
    }
    else
    {
        r.W = 0.0;
    }
}

void normalizeReservoir(inout Reservoir r)
{
    r.weightSum /= float(max(r.M, 1));
    r.M = 1;
}

// Note: W must be recalculated after a sequence of updates
bool updateReservoir(inout Reservoir r, uint lightIndex, float weight, float rnd)
{
    r.weightSum += weight;
    r.M += 1;
    if (rnd * r.weightSum < weight)
    {
        r.selected = lightIndex;
        return true;
    }
    return false;
}

// Note: W must be recalculated for combined reservoir after a sequence of updates
void updateCombinedReservoir(inout Reservoir combined, const Reservoir b, float targetPdf_b, float rnd)
{
    // targetPdf_b is for pixel q,
    // but b.W was calculated for pixel q'
    // so need to renormalize weight
    float weight = targetPdf_b * b.W * b.M;

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
    if (isinf(r.weightSum) || isnan(r.weightSum))
    {
        return uvec4(
            LIGHT_INDEX_NONE,
            floatBitsToUint(0.0),
            floatBitsToUint(0.0),
            floatBitsToUint(0.0)
        );
    }

    return uvec4(
        r.selected,
        floatBitsToUint(r.weightSum),
        floatBitsToUint(r.M),
        floatBitsToUint(r.W)
    );
}

Reservoir unpackReservoir(const uvec4 p)
{
    Reservoir r;
    r.selected = p.x;
    r.weightSum = uintBitsToFloat(p.y);
    r.M = uintBitsToFloat(p.z);
    r.W = uintBitsToFloat(p.w);
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


// Assuming that M=1
uvec4 packReservoir_Initial(const Reservoir r)
{
    if (isinf(r.weightSum) || isnan(r.weightSum))
    {
        return uvec4(
            LIGHT_INDEX_NONE,
            floatBitsToUint(0.0),
            floatBitsToUint(0.0),
            floatBitsToUint(0.0)
        );
    }

    return uvec4(
        r.selected,
        floatBitsToUint(r.weightSum),
        floatBitsToUint(r.selected_targetPdf),
        floatBitsToUint(r.W)
    );
}

Reservoir unpackReservoir_Initial(const uvec4 p)
{
    Reservoir r;
    r.selected = p.x;
    r.weightSum = uintBitsToFloat(p.y);
    r.selected_targetPdf = uintBitsToFloat(p.z);
    r.W = uintBitsToFloat(p.w);
    r.M = 1;
    return r;
}

void imageStoreReservoirInitial(const Reservoir normalized, const ivec2 pix)
{
    imageStore(framebufReservoirsInitial, pix, packReservoir_Initial(normalized));
}

Reservoir imageLoadReservoirInitial(const ivec2 pix)
{
    return unpackReservoir_Initial(imageLoad(framebufReservoirsInitial, pix));
}
#endif // DESC_SET_FRAMEBUFFERS

#endif // RESERVOIR_H_