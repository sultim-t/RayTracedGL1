#version 460

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in flat uvec3 textureIds;

layout (location = 0) out vec4 outColor;

#define DESC_SET_TEXTURES 0
#include "ShaderCommonGLSLFunc.h"

void main()
{
    outColor = color;

    for (uint i = 0; i < 3; i++)
    {
        if (textureIds[i] > 0)
        {
            outColor *= getTextureSample(textureIds[i], texCoord);
        }
    }
}