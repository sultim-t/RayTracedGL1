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
#define LIGHT_GRID_CELL_SAMPLING_OFFSET_MULTIPLIER 1.0


vec3 getGridDelta()
{
    return vec3(globalUniform.cellWorldSize);
}

vec3 getGridWholeSize()
{
    return getGridDelta() * vec3(LIGHT_GRID_SIZE_X, LIGHT_GRID_SIZE_Y, LIGHT_GRID_SIZE_Z);
}

float getCellRadius()
{
    return length(getGridDelta()) * 0.5;
}

vec3 getGridCenter()
{
    // offset a bit, so camera is in the center of the cell
    return globalUniform.cameraPosition.xyz + getGridDelta() * 0.5;
}

vec3 getGridMinExtentWorld()
{
    return getGridCenter() - getGridWholeSize() * 0.5;
}

vec3 getGridMaxExtentWorld()
{
    return getGridCenter() + getGridWholeSize() * 0.5;
}

bool isInsideCell(const vec3 worldPos)
{
    return 
        all(greaterThan(worldPos, getGridMinExtentWorld())) && 
        all(lessThan(worldPos, getGridMaxExtentWorld()));
}

vec3 jitterPositionForLightGrid(const vec3 surfPosition, const vec3 rnd)
{
    return clamp(
        surfPosition + (rnd * 2.0 - 1.0) * getCellRadius() * LIGHT_GRID_CELL_SAMPLING_OFFSET_MULTIPLIER,
        getGridMinExtentWorld(),
        getGridMaxExtentWorld());
}


vec3 getCellWorldCenter(const ivec3 cellIndex)
{
    return getGridMinExtentWorld() + getGridDelta() * (vec3(cellIndex) + 0.5);
}

ivec3 worldToCell(const vec3 worldPos)
{
    return clamp(
        ivec3((worldPos - getGridMinExtentWorld()) / getGridDelta()),
        ivec3(0),
        ivec3(LIGHT_GRID_SIZE_X, LIGHT_GRID_SIZE_Y, LIGHT_GRID_SIZE_Z));
}


int cellToArrayIndex(ivec3 cellIndex)
{
    return LIGHT_GRID_CELL_SIZE * (
        cellIndex.x +
        cellIndex.y * LIGHT_GRID_SIZE_X +
        cellIndex.z * LIGHT_GRID_SIZE_X * LIGHT_GRID_SIZE_Y);
}

ivec3 arrayIndexToCell(int arrayIndex)
{
    int c = arrayIndex / LIGHT_GRID_CELL_SIZE;
    
    ivec3 cellIndex = 
    {
        (c % (LIGHT_GRID_SIZE_X)),
        (c % (LIGHT_GRID_SIZE_X * LIGHT_GRID_SIZE_Y)) / LIGHT_GRID_SIZE_X,
        (c / (LIGHT_GRID_SIZE_X * LIGHT_GRID_SIZE_Y)),
    };

    return cellIndex;
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