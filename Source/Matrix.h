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

#include "RTGL1/RTGL1.h"

namespace RTGL1
{
namespace Matrix
{
    void Multiply( float* result, const float* a, const float* b );
    void Inverse( float* inversed, const float* m );
    void Transpose( float* transposed, const float* m );
    void Transpose( float t[ 4 ][ 4 ] );

    void ToMat4( float* result, const RgTransform& m );
    void ToMat4Transposed( float* result, const RgTransform& m );

    void GetViewMatrix( float* result, const float* pos, float pitch, float yaw, float roll );
    void GetCubemapViewProjMat(
        float* result, uint32_t sideIndex, const float* position, float zNear, float zFar );
    // Set new position for viewer in (column-major) view matrix.
    void SetNewViewerPosition( float* result, const float* viewMatrix, const float* newPosition );

    void MakeProjectionMatrix(
        float* matrix, float aspect, float fovYRad, float zNear, float zFar );
}
}
