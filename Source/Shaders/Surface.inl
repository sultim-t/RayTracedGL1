
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

#ifndef SURFACE_INL_
#define SURFACE_INL_

#include "BRDF.h"


struct Surface
{
    vec3    position;
    uint    instCustomIndex;
    vec3    normalGeom;
    float   roughness;
    vec3    normal;
    vec3    albedo;
    bool    isSky;
    vec3    specularColor;
    float   emission;
    vec3    toViewerDir;
};


#if defined(DESC_SET_FRAMEBUFFERS)
#if !defined(FRAMEBUF_IGNORE_ATTACHMENTS)
#if defined(CHECKERBOARD_FULL_WIDTH) && defined(CHECKERBOARD_FULL_HEIGHT)

Surface fetchGbufferSurface(const ivec2 pix)
{
    Surface s;
    s.isSky = isSkyPix(pix);

    if (s.isSky)
    {
        return s;
    }
    
    // framebufAlbedo ALWAYS uses regular layout because of the sky rasterization pass  
    s.albedo = texelFetch(framebufAlbedo_Sampler, getRegularPixFromCheckerboardPix(pix), 0).rgb;
    s.emission = getLuminance(texelFetch(framebufScreenEmission_Sampler, getRegularPixFromCheckerboardPix(pix), 0).rgb);   
    {
        vec4 posEnc             = texelFetch(framebufSurfacePosition_Sampler, pix, 0);
        s.position              = posEnc.xyz;
        s.instCustomIndex       = floatBitsToUint(posEnc.a);
    }
    {
        vec2 metallicRoughness  = texelFetch(framebufMetallicRoughness_Sampler, pix, 0).xy;
        s.specularColor         = getSpecularColor(s.albedo, metallicRoughness[0]);
        s.roughness             = metallicRoughness[1];
    }
    s.normalGeom                = texelFetchNormalGeometry(pix);
    s.normal                    = texelFetchNormal(pix);
    s.toViewerDir               = -texelFetch(framebufViewDirection_Sampler, pix, 0).xyz;
    return s;
}

Surface fetchGbufferSurface_NoAlbedoViewDir_Prev(const ivec2 pix)
{
    Surface s;
    s.isSky = false;
    s.albedo = vec3(1.0);
    s.emission = 0.0;
    {
        vec4 posEnc             = texelFetch(framebufSurfacePosition_Prev_Sampler, pix, 0);
        s.position              = posEnc.xyz;
        s.instCustomIndex       = floatBitsToUint(posEnc.a);
    }
    {
        vec2 metallicRoughness  = texelFetch(framebufMetallicRoughness_Prev_Sampler, pix, 0).xy;
        s.specularColor         = getSpecularColor(s.albedo, metallicRoughness[0]);
        s.roughness             = metallicRoughness[1];
    }
    s.normalGeom                = texelFetchNormalGeometry_Prev(pix);
    s.normal                    = texelFetchNormal_Prev(pix);
    s.toViewerDir               = vec3(0.0);
    return s;
}
#endif // CHECKERBOARD_FULL_WIDTH && CHECKERBOARD_FULL_HEIGHT
#endif // !FRAMEBUF_IGNORE_ATTACHMENTS
#endif // DESC_SET_FRAMEBUFFERS
       

Surface hitInfoToSurface_Indirect(const ShHitInfo h, const vec3 rayDirection)
{
    Surface s;
    s.position = h.hitPosition;
    s.instCustomIndex = h.instCustomIndex;
    s.normalGeom = h.normalGeom;
    s.roughness = h.roughness;
    s.normal = h.normalGeom; // ignore precise normals for indirect
    s.albedo = h.albedo;
    s.isSky = false;
    s.specularColor = getSpecularColor(h.albedo, h.metallic);
    s.emission = h.emission;
    s.toViewerDir = -rayDirection;
    return s;
}
       
#endif // SURFACE_INL_
