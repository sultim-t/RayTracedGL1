#pragma once
#include "Common.h"

struct VertexBufferProperties
{
    bool vertexArrayOfStructs;
    uint32_t positionStride;
    uint32_t normalStride;
    uint32_t texCoordStride;
    uint32_t colorStride;
};