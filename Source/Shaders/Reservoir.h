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
    uint selected;
    float weightSum;
    float M;
};

uvec4 packReservoir(const Reservoir r)
{
    return uvec4(
        r.selected,
        floatBitsToUint(r.weightSum),
        floatBitsToUint(r.M),
        0
    );
}

Reservoir unpackReservoir(const uvec4 p)
{
    Reservoir r;
    r.selected = p.x;
    r.weightSum = uintBitsToFloat(p.y);
    r.M = uintBitsToFloat(p.z);
    return r;
}

Reservoir emptyReservoir()
{
    Reservoir r;
    r.selected = UINT32_MAX;
    r.weightSum = 0.0;
    r.M = 0.0;
    return r;
}

void updateReservoir(inout Reservoir r, uint lightIndex, float weight, float rnd)
{
    r.weightSum += weight;
    r.M += 1;
    if (rnd * r.weightSum < weight)
    {
        r.selected = lightIndex;
    }
}

float calcReservoirW(const Reservoir r, float targetPdf_selected)
{
    if (targetPdf_selected <= 0.00001 || r.M <= 0)
    {
        return 0.0;
    }

    return 1.0 / targetPdf_selected * (r.weightSum / r.M);
}

/*Reservoir combineReservoirs(const Reservoir a, const Reservoir b, const Surface surf, const vec2 pointRnd)
{
    Reservoir combined = emptyReservoir();

    updateReservoir(combined, a.selected, targetPdfForLightSample(a.selected, surf, pointRnd) * a.W * a.M);
    updateReservoir(combined, b.selected, targetPdfForLightSample(b.selected, surf, pointRnd) * b.W * b.M);
    
    combined.M = a.M + b.M;

    combined.W = calcReservoirW(combined, targetPdfForLightSample(combined.selected));

    return combined;
}*/

#endif // RESERVOIR_H_