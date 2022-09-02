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

#ifndef VOLUMETRIC_H_
#define VOLUMETRIC_H_

#if !defined( DESC_SET_GLOBAL_UNIFORM ) || !defined( DESC_SET_VOLUMETRIC )
    #error
#endif


vec3 volume_getCenter_T( const ivec3 cell, const mat4 viewprojInv, const vec3 origin )
{
    vec3 local =
        ( vec3( cell ) + 0.5 ) / vec3( VOLUMETRIC_SIZE_X, VOLUMETRIC_SIZE_Y, VOLUMETRIC_SIZE_Z );

    vec4 ndc = {
        local.x * 2.0 - 1.0,
        local.y * 2.0 - 1.0,
        0.1,
        1.0,
    };

    vec4 worldpos = viewprojInv * ndc;
    worldpos.xyz /= worldpos.w;

    vec3 worlddir = normalize( worldpos.xyz - origin );

    float n = globalUniform.volumeCameraNear;
    float f = globalUniform.volumeCameraFar;

    float z    = clamp( local.z, 0.0, 1.0 );
    z          = square( z );
    float dist = mix( n, f, z );

    return origin + worlddir * dist;
}

vec3 volume_toSamplePosition_T( const vec3 world, const mat4 viewproj, const vec3 origin )
{
    vec4 ndc = viewproj * vec4( world, 1.0 );
    ndc.xy /= ndc.w;

    float n = globalUniform.volumeCameraNear;
    float f = globalUniform.volumeCameraFar;

    float dist = length( world - origin );
    float z    = ( dist - n ) / ( f - n );
    z          = clamp( z, 0.0, 1.0 );
    z          = sqrt( z );

    return vec3( 
        ndc.x * 0.5 + 0.5,
        ndc.y * 0.5 + 0.5,
        z );
}


vec3 volume_getCenter( const ivec3 cell ) 
{
    return volume_getCenter_T(
        cell, globalUniform.volumeViewProjInv, globalUniform.cameraPosition.xyz );
}
vec3 volume_getCenter_Prev( const ivec3 cell )
{
    return volume_getCenter_T(
        cell, globalUniform.volumeViewProjInv_Prev, globalUniform.cameraPositionPrev.xyz );
}


vec4 volume_sample( const vec3 world ) 
{
    vec3 sp = volume_toSamplePosition_T(
        world, globalUniform.volumeViewProj, globalUniform.cameraPosition.xyz );

    return textureLod( g_volumetric_Sampler, sp, 0.0 );
}

vec4 volume_sample_Prev( const ivec3 curcell )
{
    vec3 curworld = volume_getCenter( curcell );

    vec3 spPrev = volume_toSamplePosition_T(
        curworld, globalUniform.volumeViewProj_Prev, globalUniform.cameraPositionPrev.xyz );

    return textureLod( g_volumetric_Sampler_Prev, spPrev, 0.0 );
}


ivec3 volume_toCellIndex( const vec3 world )
{
    vec3 sp = volume_toSamplePosition_T(
        world, globalUniform.volumeViewProj, globalUniform.cameraPosition.xyz );

    return ivec3( sp * vec3( VOLUMETRIC_SIZE_X, VOLUMETRIC_SIZE_Y, VOLUMETRIC_SIZE_Z ) );
}


#endif // VOLUMETRIC_H_