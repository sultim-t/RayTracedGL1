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

#version 460

#define DESC_SET_GLOBAL_UNIFORM 0
#define DESC_SET_DECALS 3
#include "ShaderCommonGLSLFunc.h"

layout (location = 0) flat out uint outInstanceIndex;

// Buffer-free [-0.5, 0.5] cube triangle strips 
vec4 getPosition()
{
    // https://twitter.com/donzanoid/status/616370134278606848
    int b = 1 << (gl_VertexIndex % 14);

    return vec4(
        float((0x287A & b) != 0) - 0.5,
        float((0x02AF & b) != 0) - 0.5,
        float((0x31E3 & b) != 0) - 0.5,
        1.0
    );
}

void main()
{
    outInstanceIndex = gl_InstanceIndex;
    gl_Position = globalUniform.projection * globalUniform.view * decalInstances[gl_InstanceIndex].transform * getPosition();
}