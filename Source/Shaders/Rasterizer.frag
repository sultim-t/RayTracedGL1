#version 460

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texCoord;

layout (location = 0) out vec4 outColor;

#define DESC_SET_TEXTURES 0
#include "ShaderCommonGLSLFunc.h"

layout(push_constant) uniform RasterizerFrag_BT 
{
    layout(offset = 64) uvec3 textureIndices;
} rasterizerFragInfo;

void main()
{
    outColor = color;

    for (uint i = 0; i < 3; i++)
    {
        if (rasterizerFragInfo.textureIndices[i] > 0)
        {
            outColor *= getTextureSample(rasterizerFragInfo.textureIndices[i], texCoord);
        }
    }
}