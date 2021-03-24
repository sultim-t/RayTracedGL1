// Copyright (c) 2021 Sultim Tsyrendashiev
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

#version 460

layout(origin_upper_left) in vec4 gl_FragCoord;

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texCoord;

layout (location = 0) out vec4 outColor;

#define DESC_SET_TEXTURES 0
#define DESC_SET_FRAMEBUFFERS 1
#include "ShaderCommonGLSLFunc.h"

layout(push_constant) uniform RasterizerFrag_BT 
{
    layout(offset = 64) uint textureIndex;
} rasterizerFragInfo;

void main()
{
    const ivec2 pix = ivec2(gl_FragCoord.xy);

    const float depth = gl_FragCoord.z;
    const float sceneDepth = texelFetch(framebufDepth_Sampler, pix, 0).w;

    if (sceneDepth <= depth)
    {
        discard;
    }

    outColor = color * getTextureSample(rasterizerFragInfo.textureIndex, texCoord);
}