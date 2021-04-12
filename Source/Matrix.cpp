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

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "Matrix.h"

#include <cstring>
#include <cmath>

using namespace RTGL1;

constexpr float M_PI = 3.141592653589793238462643383279f;

void Matrix::Inverse(float *inversed, const float *m)
{
    // Copied from "Mesa - The 3D Graphics Library" (MIT license)
    // https://github.com/mesa3d/mesa/blob/master/src/mesa/math/m_matrix.c

    float inv[16];

    inv[0] = m[5] * m[10] * m[15] -
        m[5] * m[11] * m[14] -
        m[9] * m[6] * m[15] +
        m[9] * m[7] * m[14] +
        m[13] * m[6] * m[11] -
        m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] +
        m[4] * m[11] * m[14] +
        m[8] * m[6] * m[15] -
        m[8] * m[7] * m[14] -
        m[12] * m[6] * m[11] +
        m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] -
        m[4] * m[11] * m[13] -
        m[8] * m[5] * m[15] +
        m[8] * m[7] * m[13] +
        m[12] * m[5] * m[11] -
        m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] +
        m[4] * m[10] * m[13] +
        m[8] * m[5] * m[14] -
        m[8] * m[6] * m[13] -
        m[12] * m[5] * m[10] +
        m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] +
        m[1] * m[11] * m[14] +
        m[9] * m[2] * m[15] -
        m[9] * m[3] * m[14] -
        m[13] * m[2] * m[11] +
        m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] -
        m[0] * m[11] * m[14] -
        m[8] * m[2] * m[15] +
        m[8] * m[3] * m[14] +
        m[12] * m[2] * m[11] -
        m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] +
        m[0] * m[11] * m[13] +
        m[8] * m[1] * m[15] -
        m[8] * m[3] * m[13] -
        m[12] * m[1] * m[11] +
        m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] -
        m[0] * m[10] * m[13] -
        m[8] * m[1] * m[14] +
        m[8] * m[2] * m[13] +
        m[12] * m[1] * m[10] -
        m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] -
        m[1] * m[7] * m[14] -
        m[5] * m[2] * m[15] +
        m[5] * m[3] * m[14] +
        m[13] * m[2] * m[7] -
        m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] +
        m[0] * m[7] * m[14] +
        m[4] * m[2] * m[15] -
        m[4] * m[3] * m[14] -
        m[12] * m[2] * m[7] +
        m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] -
        m[0] * m[7] * m[13] -
        m[4] * m[1] * m[15] +
        m[4] * m[3] * m[13] +
        m[12] * m[1] * m[7] -
        m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] +
        m[0] * m[6] * m[13] +
        m[4] * m[1] * m[14] -
        m[4] * m[2] * m[13] -
        m[12] * m[1] * m[6] +
        m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
        m[1] * m[7] * m[10] +
        m[5] * m[2] * m[11] -
        m[5] * m[3] * m[10] -
        m[9] * m[2] * m[7] +
        m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
        m[0] * m[7] * m[10] -
        m[4] * m[2] * m[11] +
        m[4] * m[3] * m[10] +
        m[8] * m[2] * m[7] -
        m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
        m[0] * m[7] * m[9] +
        m[4] * m[1] * m[11] -
        m[4] * m[3] * m[9] -
        m[8] * m[1] * m[7] +
        m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
        m[0] * m[6] * m[9] -
        m[4] * m[1] * m[10] +
        m[4] * m[2] * m[9] +
        m[8] * m[1] * m[6] -
        m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
    {
        inversed[i] = inv[i] * det;
    }
}

void Matrix::Transpose(float *transposed, const float *m)
{
    const float(&src)[4][4] = *reinterpret_cast<const float(*)[4][4]>(m);
    float(&dst)[4][4] = *reinterpret_cast<float(*)[4][4]>(transposed);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            dst[i][j] = src[j][i];
        }
    }
}

void Matrix::Multiply(float *result, const float *a, const float *b)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result[i * 4 + j] =
                a[i * 4 + 0] * b[0 * 4 + j] +
                a[i * 4 + 1] * b[1 * 4 + j] +
                a[i * 4 + 2] * b[2 * 4 + j] +
                a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
}

void Matrix::ToMat4(float *result, const RgTransform &m)
{
    result[0] = m.matrix[0][0];
    result[1] = m.matrix[0][1];
    result[2] = m.matrix[0][2];

    result[4] = m.matrix[1][0];
    result[5] = m.matrix[1][1];
    result[6] = m.matrix[1][2];

    result[8] = m.matrix[2][0];
    result[9] = m.matrix[2][1];
    result[10] = m.matrix[2][2];

    result[3] = m.matrix[0][3];
    result[7] = m.matrix[1][3];
    result[11] = m.matrix[2][3];

    result[12] = 0.0f;
    result[13] = 0.0f;
    result[14] = 0.0f;
    result[15] = 1.0f;
}

void Matrix::ToMat4Transposed(float *result, const RgTransform &m)
{
    result[0] = m.matrix[0][0];
    result[4] = m.matrix[0][1];
    result[8] = m.matrix[0][2];

    result[1] = m.matrix[1][0];
    result[5] = m.matrix[1][1];
    result[9] = m.matrix[1][2];

    result[2] = m.matrix[2][0];
    result[6] = m.matrix[2][1];
    result[10] = m.matrix[2][2];

    result[12] = m.matrix[0][3];
    result[13] = m.matrix[1][3];
    result[14] = m.matrix[2][3];

    result[3] = 0.0f;
    result[7] = 0.0f;
    result[11] = 0.0f;
    result[15] = 1.0f;
}

static float Dot3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void GetViewMatrix(float *result, const float *pos, float pitch, float yaw, float roll)
{
    float fSinH = std::sin(yaw); 
    float fCosH = std::cos(yaw);
    float fSinP = std::sin(pitch); 
    float fCosP = std::cos(pitch);
    float fSinB = std::sin(roll); 
    float fCosB = std::cos(roll);

    // inverse transform, i.e. (T * R)^-1 = R^(-1) * T^(-1) = R^(-1) * T^(-1)
    float m[4][4];

    // rotation matrix inverse
    m[0][0] = fCosH*fCosB+fSinP*fSinH*fSinB;
    m[1][0] = fSinP*fSinH*fCosB-fCosH*fSinB;
    m[2][0] = fCosP*fSinH;
    m[0][1] = fCosP*fSinB;
    m[1][1] = fCosP*fCosB;
    m[2][1] = -fSinP;
    m[0][2] = fSinP*fCosH*fSinB-fSinH*fCosB;
    m[1][2] = fSinP*fCosH*fCosB+fSinH*fSinB;
    m[2][2] = fCosP*fCosH;

    // flip Y axis for Vulkan
    m[1][0] = -m[1][0];
    m[1][1] = -m[1][1];
    m[1][2] = -m[1][2];

    float invT[] = { -pos[0], -pos[1], -pos[2] };

    // 4th column
    m[0][3] = Dot3(m[0], invT);
    m[1][3] = Dot3(m[1], invT);
    m[2][3] = Dot3(m[2], invT);

    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = 0;
    m[3][3] = 1;

    // to column-major
    Matrix::Transpose(result, (float*)m);
}

void Matrix::GetCubemapViewProjMat(float *result, uint32_t sideIndex, const float *position, const float *proj)
{
    float view[16];

    switch (sideIndex)
    {
    // POSITIVE_X
    case 0:
        GetViewMatrix(view, position, 0, M_PI / 2, 0);
        break;

    // NEGATIVE_X
    case 1:
        GetViewMatrix(view, position, 0, -M_PI / 2, 0);
        break;

    // POSITIVE_Y
    case 2:
        GetViewMatrix(view, position, -M_PI / 2, 0, 0);
        break;

    // NEGATIVE_Y
    case 3:
        GetViewMatrix(view, position, M_PI / 2, 0, 0);
        break;

    // POSITIVE_Z
    case 4:
        GetViewMatrix(view, position, 0, 0, 0);
        break;

    // NEGATIVE_Z
    case 5:
        GetViewMatrix(view, position, 0, M_PI, 0);
        break;
    }

    Multiply(result, view, proj);
}
