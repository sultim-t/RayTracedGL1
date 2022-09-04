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

layout (location = 0) in vec4 vertColor;
layout (location = 1) in vec2 vertTexCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outScreenEmission;

#define DESC_SET_TEXTURES 0
#define DESC_SET_TONEMAPPING 1
#include "ShaderCommonGLSLFunc.h"

layout(push_constant) uniform RasterizerFrag_BT 
{
    layout(offset = 64) vec4 color;
    layout(offset = 80) uint textureIndex;
    layout(offset = 84) uint emissionTextureIndex;
    layout(offset = 88) float emissionMultiplier;
} rasterizerFragInfo;

layout (constant_id = 0) const uint alphaTest = 0;

#define ALPHA_THRESHOLD 0.5
#define EMISSION_CHANNEL 2


vec3 uncharted2TonemapOp( const vec3 x )
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - E / F;
}

vec3 uncharted2Tonemap( const vec3 color, float whitePoint )
{
    return uncharted2TonemapOp( 2.0 * color ) / uncharted2TonemapOp( vec3( whitePoint ) );
}

void main()
{
    vec4 albedoAlpha = getTextureSample(rasterizerFragInfo.textureIndex, vertTexCoord);
    float emission = getTextureSample(rasterizerFragInfo.emissionTextureIndex, vertTexCoord)[EMISSION_CHANNEL];

    outColor = rasterizerFragInfo.color * vertColor * albedoAlpha;
    outColor.rgb = uncharted2Tonemap( 1/getExposure()* outColor.rgb, 1.0 );

    outScreenEmission = vec3( 0 );
    //emission* albedoAlpha.rgb* rasterizerFragInfo.emissionMultiplier;

    if (alphaTest != 0)
    {
        if (outColor.a < ALPHA_THRESHOLD)
        {
            discard;
        }
    }
}