// Copyright (c) 2021-2022 Sultim Tsyrendashiev
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

#ifndef LIGHT_GRID_H_
#define LIGHT_GRID_H_

#include "Reservoir.h"


#define LIGHT_GRID_INITIAL_SAMPLES 8
#define LIGHT_GRID_TEMPORAL 1


int cellToArrayIndex(const ivec3 cellIndex)
{
    return LIGHT_GRID_CELL_SIZE * (
        cellIndex.x +
        cellIndex.z * LIGHT_GRID_SIZE_HORIZONTAL_X +
        cellIndex.y * LIGHT_GRID_SIZE_HORIZONTAL_X * LIGHT_GRID_SIZE_HORIZONTAL_Z);
}

ivec3 arrayIndexToCell(int arrayIndex)
{
    int c = arrayIndex / LIGHT_GRID_CELL_SIZE;
    
    ivec3 cellIndex;
    cellIndex.x = (c % (LIGHT_GRID_SIZE_HORIZONTAL_X));
    cellIndex.z = (c % (LIGHT_GRID_SIZE_HORIZONTAL_X * LIGHT_GRID_SIZE_HORIZONTAL_Z)) / LIGHT_GRID_SIZE_HORIZONTAL_X;
    cellIndex.y = (c / (LIGHT_GRID_SIZE_HORIZONTAL_X * LIGHT_GRID_SIZE_HORIZONTAL_Z));

    return cellIndex;
}

// TODO
#define CENTER (vec3(0))
#define SIZE (vec3(100, 50, 100)*10)
#define BASE (CENTER - SIZE * 0.5)
#define DELTA (SIZE / vec3(LIGHT_GRID_SIZE_HORIZONTAL_X, LIGHT_GRID_SIZE_VERTICAL_Y, LIGHT_GRID_SIZE_HORIZONTAL_Z))

float getCellRadius()
{
    return length(DELTA) * 0.5;
}

bool isInsideCell(const vec3 worldPos)
{
    return all(greaterThan(worldPos, BASE)) && all(lessThan(worldPos, BASE + SIZE));
}

vec3 cellToWorld(const ivec3 cellIndex)
{
    return BASE + DELTA * (vec3(cellIndex) + 0.5);
}

ivec3 worldToCell(const vec3 worldPos)
{
    return ivec3((worldPos - BASE) / DELTA - 0.5);
}

Reservoir unpackReservoirFromLightGrid(const ShLightInCell s)
{
    Reservoir r;
    r.selected = s.selected_lightIndex;
    r.selected_targetPdf = s.selected_targetPdf;
    r.weightSum = s.weightSum;
    r.M = 1;
    return r;
}

// normalized.M must be 1
ShLightInCell packReservoirToLightGrid(const Reservoir normalized)
{
    ShLightInCell s;
    s.selected_lightIndex = normalized.selected;
    s.selected_targetPdf = normalized.selected_targetPdf;
    s.weightSum = normalized.weightSum;
    return s;
}

#endif // LIGHT_GRID_H_