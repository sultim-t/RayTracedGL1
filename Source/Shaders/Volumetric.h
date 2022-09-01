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


vec3 volume_getOrigin()
{
    return globalUniform.cameraPosition.xyz;
}

vec3 volume_getCenter( const ivec3 cell )
{
    vec3 local =
        ( vec3( cell ) + 0.5 ) / vec3( VOLUMETRIC_SIZE_X, VOLUMETRIC_SIZE_Y, VOLUMETRIC_SIZE_Z );

    vec4 ndc = {
        local.x * 2.0 - 1.0,
        local.y * 2.0 - 1.0,
        0.1,
        1.0,
    };

    vec4 worldpos = globalUniform.volumeViewProjInv * ndc;
    worldpos.xyz /= worldpos.w;

    vec3 worlddir = normalize( worldpos.xyz - volume_getOrigin() );

    float n = globalUniform.volumeCameraNear;
    float f = globalUniform.volumeCameraFar;

    float z    = clamp( local.z, 0.0, 1.0 );
    z          = square( z );
    float dist = mix( n, f, z );

    return volume_getOrigin() + worlddir * dist;
}

vec3 volume_toSamplePosition( const vec3 world )
{
    vec4 ndc = globalUniform.volumeViewProj * vec4( world, 1.0 );
    ndc.xy /= ndc.w;

    float n = globalUniform.volumeCameraNear;
    float f = globalUniform.volumeCameraFar;

    float dist = length( world - volume_getOrigin() );
    float z    = ( dist - n ) / ( f - n );
    z          = clamp( z, 0.0, 1.0 );
    z          = sqrt( z );

    return vec3( 
        ndc.x * 0.5 + 0.5,
        ndc.y * 0.5 + 0.5,
        z );
}

vec4 volume_sample( const vec3 world ) 
{
    return textureLod( g_volumetric_Sampler, volume_toSamplePosition( world ), 0.0 );
}

ivec3 volume_toCellIndex( const vec3 world )
{
    return ivec3( volume_toSamplePosition( world ) *
                  vec3( VOLUMETRIC_SIZE_X, VOLUMETRIC_SIZE_Y, VOLUMETRIC_SIZE_Z ) );
}


#endif // VOLUMETRIC_H_