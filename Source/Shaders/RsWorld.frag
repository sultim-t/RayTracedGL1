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

#define DESC_SET_TEXTURES       0
#define DESC_SET_GLOBAL_UNIFORM 1
#define DESC_SET_TONEMAPPING    2
#define DESC_SET_VOLUMETRIC     3
#include "ShaderCommonGLSLFunc.h"
#include "Exposure.h"
#include "Volumetric.h"

layout(push_constant) uniform RasterizerFrag_BT 
{
    layout(offset = 64) vec4 color;
    layout(offset = 80) uint textureIndex;
    layout(offset = 84) uint emissionTextureIndex;
} rasterizerFragInfo;

layout (constant_id = 0) const uint alphaTest = 0;

#define ALPHA_THRESHOLD 0.5
#define EMISSION_CHANNEL 2


void main()
{
    vec4 albedoAlpha = getTextureSample( rasterizerFragInfo.textureIndex, vertTexCoord );
    outColor         = rasterizerFragInfo.color * vertColor * albedoAlpha;

    if( globalUniform.lightmapEnable == 0 )
    {
#if ILLUMINATION_VOLUME
        vec4 ndc = vec4( gl_FragCoord.xyz, 1.0 );
        ndc.xy   /= vec2( globalUniform.renderWidth, globalUniform.renderHeight );
        ndc.xy = ndc.xy * 2.0 - 1.0;

        vec4 worldpos = globalUniform.invView * globalUniform.invProjection * ndc;
        worldpos.xyz /= worldpos.w;

        vec3 sp = volume_toSamplePosition_T(
            worldpos.xyz, globalUniform.volumeViewProj, globalUniform.cameraPosition.xyz );
        vec3 illum = textureLod( g_illuminationVolume_Sampler, sp, 0.0 ).rgb;

        outColor.rgb *= illum;
#else
        outColor.rgb *= ev100ToLuminousExposure( getCurrentEV100() );
#endif
    }

    float emis = getTextureSample( rasterizerFragInfo.emissionTextureIndex, vertTexCoord )[ EMISSION_CHANNEL ];
    outScreenEmission = rmeEmissionToScreenEmission( emis ) * albedoAlpha.rgb;

    if( alphaTest != 0 )
    {
        if( outColor.a < ALPHA_THRESHOLD )
        {
            discard;
        }
    }
}