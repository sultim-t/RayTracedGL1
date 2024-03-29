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

layout( location = 0 ) in vec3 in_position;
layout( location = 1 ) in vec4 in_color;
layout( location = 2 ) in vec2 in_texCoord;

layout( location = 0 ) out vec4 out_color;
layout( location = 1 ) out vec2 out_texCoord;
layout( location = 2 ) out uint out_textureIndex;
layout( location = 3 ) out uint out_emissiveTextureIndex;
layout( location = 4 ) out float out_emissiveMult;

#define DESC_SET_LENS_FLARE_VERTEX_INSTANCES 1
#include "ShaderCommonGLSLFunc.h"

layout( set     = DESC_SET_LENS_FLARE_VERTEX_INSTANCES,
        binding = BINDING_DRAW_LENS_FLARES_INSTANCES ) buffer LensFlareInstances_BT
{
    ShLensFlareInstance lensFlareInstances[];
};

layout( push_constant ) uniform RasterizerVert_BT
{
    layout( offset = 0 ) mat4 viewProj;
}
rasterizerVertInfo;

layout( constant_id = 0 ) const uint applyVertexColorGamma = 0;

vec4 baseColor( uint instPackedColor )
{
    if( applyVertexColorGamma != 0 )
    {
        return vec4( pow( in_color.rgb, vec3( 2.2 ) ), in_color.a ) *
               unpackUintColor( instPackedColor );
    }
    else
    {
        return in_color * unpackUintColor( instPackedColor );
    }
}

void main()
{
    const ShLensFlareInstance inst = lensFlareInstances[ gl_InstanceIndex ];

    out_color                = baseColor( inst.packedColor );
    out_texCoord             = in_texCoord;
    out_textureIndex         = inst.textureIndex;
    out_emissiveTextureIndex = inst.emissiveTextureIndex;
    out_emissiveMult         = inst.emissiveMult;

    gl_Position = rasterizerVertInfo.viewProj * vec4( in_position, 1.0 );
}
