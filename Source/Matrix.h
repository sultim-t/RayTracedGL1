#pragma once

#include "RTGL1/RTGL1.h"

class Matrix
{
public:
    static void Inverse(float *inversed, const float *m);
    static void Transpose(float *transposed, const float *m);
    static void ToMat4(float *result, const RgTransform &m);
    static void ToMat4Transposed(float *result, const RgTransform &m);
};