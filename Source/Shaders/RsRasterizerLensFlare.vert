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

#if 0

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;
layout (location = 2) in vec2 texCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out uint outTextureIndex;

#define DESC_SET_GLOBAL_UNIFORM 0
#define DESC_SET_LENS_FLARE_VERTEX_INSTANCES 2
#include "ShaderCommonGLSLFunc.h"

layout(set = DESC_SET_LENS_FLARE_VERTEX_INSTANCES, binding = BINDING_DRAW_LENS_FLARES_INSTANCES) buffer LensFlareInstances_BT
{
    ShLensFlareInstance lensFlareInstances[];
};


layout (constant_id = 0) const uint applyVertexColorGamma = 0;

void main()
{
    if (applyVertexColorGamma != 0)
    {
        outColor = vec4(pow(color.rgb, vec3(2.2)), color.a);
    }
    else
    {
        outColor = color;
    }

    outTexCoord = texCoord;
    outTextureIndex = lensFlareInstances[gl_InstanceIndex].textureIndex;

    if (globalUniform.applyViewProjToLensFlares != 0)
    {
        gl_Position = globalUniform.projection * globalUniform.view * vec4(position, 1.0);
    }
    else
    {
        gl_Position = vec4(position, 1.0);
    }
}

#else
void main() {}
#endif
