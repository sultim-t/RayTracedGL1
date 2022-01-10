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

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require

#include "Utils.h"
#include "ShaderCommonGLSL.h"
#include "Structs.h"
#include "BRDF.h"

// Functions to access RTGL data.
// Available defines:
// * DESC_SET_GLOBAL_UNIFORM    -- to access global uniform buffer
// * DESC_SET_VERTEX_DATA       -- to access geometry data;
//                                 DESC_SET_GLOBAL_UNIFORM must be defined; 
//                                 Define VERTEX_BUFFER_WRITEABLE for writing
// * DESC_SET_TEXTURES          -- to access textures by index
// * DESC_SET_FRAMEBUFFERS      -- to access framebuffers (defined in ShaderCommonGLSL.h)
// * DESC_SET_RANDOM            -- to access blue noise (uniform distribution) and sampling points on surfaces
// * DESC_SET_TONEMAPPING       -- to access histogram and average luminance;
//                                 define TONEMAPPING_BUFFER_WRITEABLE for writing



#define FAKE_ROUGH_SPECULAR_THRESHOLD 0.5
#define FAKE_ROUGH_SPECULAR_LENGTH 0.25



#ifdef DESC_SET_GLOBAL_UNIFORM
layout(
    set = DESC_SET_GLOBAL_UNIFORM,
    binding = BINDING_GLOBAL_UNIFORM)
    readonly uniform GlobalUniform_BT
{
    ShGlobalUniform globalUniform;
};
#endif // DESC_SET_GLOBAL_UNIFORM



#ifdef DESC_SET_TEXTURES
layout(
    set = DESC_SET_TEXTURES,
    binding = BINDING_TEXTURES)
    uniform sampler2D globalTextures[];

sampler2D getTexture(uint textureIndex)
{
    return globalTextures[nonuniformEXT(textureIndex)];
}

vec4 getTextureSample(uint textureIndex, const vec2 texCoord)
{
    return texture(globalTextures[nonuniformEXT(textureIndex)], texCoord);
}

vec4 getTextureSampleLod(uint textureIndex, const vec2 texCoord, float lod)
{
    return textureLod(globalTextures[nonuniformEXT(textureIndex)], texCoord, lod);
}

vec4 getTextureSampleGrad(uint textureIndex, const vec2 texCoord, const vec2 dPdx, const vec2 dPdy)
{
    return textureGrad(globalTextures[nonuniformEXT(textureIndex)], texCoord, dPdx, dPdy);
}
#endif // DESC_SET_TEXTURES



// instanceID is assumed to be < 256 (i.e. 8 bits ) and 
// instanceCustomIndexEXT is 24 bits by Vulkan spec
uint packInstanceIdAndCustomIndex(int instanceID, int instanceCustomIndexEXT)
{
    return (instanceID << 24) | instanceCustomIndexEXT;
}

ivec2 unpackInstanceIdAndCustomIndex(uint instanceIdAndIndex)
{
    return ivec2(
        instanceIdAndIndex >> 24,
        instanceIdAndIndex & 0xFFFFFF
    );
}

void unpackInstanceIdAndCustomIndex(uint instanceIdAndIndex, out int instanceId, out int instanceCustomIndexEXT)
{
    instanceId = int(instanceIdAndIndex >> 24);
    instanceCustomIndexEXT = int(instanceIdAndIndex & 0xFFFFFF);
}

uint packGeometryAndPrimitiveIndex(int geometryIndex, int primitiveIndex)
{
#if MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW + MAX_GEOMETRY_PRIMITIVE_COUNT_POW != 32
    #error The sum of MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW and MAX_GEOMETRY_PRIMITIVE_COUNT_POW must be 32\
        for packing geometry and primitive index
#endif

    return (primitiveIndex << MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW) | geometryIndex;
}

ivec2 unpackGeometryAndPrimitiveIndex(uint geomAndPrimIndex)
{
#if (1 << MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW) != MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT
    #error MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT must be (1 << MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW)
#endif

    return ivec2(
        geomAndPrimIndex >> MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW,
        geomAndPrimIndex & (MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT - 1)
    );
}

void unpackGeometryAndPrimitiveIndex(uint geomAndPrimIndex, out int geometryIndex, out int primitiveIndex)
{
#if (1 << MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW) != MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT
    #error MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT must be (1 << MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW)
#endif

    primitiveIndex = int(geomAndPrimIndex >> MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW);
    geometryIndex = int(geomAndPrimIndex & (MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT - 1));
}



#ifdef DESC_SET_GLOBAL_UNIFORM
#ifdef DESC_SET_VERTEX_DATA
    #include "VertexData.inl"
#endif
#endif



#ifdef DESC_SET_TONEMAPPING
layout(set = DESC_SET_TONEMAPPING, binding = BINDING_LUM_HISTOGRAM) 
#ifndef TONEMAPPING_BUFFER_WRITEABLE
    readonly
#endif
    buffer Historam_BT
{
    ShTonemapping tonemapping;
};
#endif // DESC_SET_TONEMAPPING



#ifdef DESC_SET_LIGHT_SOURCES
layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_SPHERICAL) readonly buffer LightSourcesSpherical_BT
{
    ShLightSpherical lightSourcesSpherical[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_SPHERICAL_PREV) readonly buffer LightSourcesSphericalPrev_BT
{
    ShLightSpherical lightSourcesSpherical_Prev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_POLYGONAL) readonly buffer LightSourcesPolygonal_BT
{
    ShLightPolygonal lightSourcesPolygonal[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_POLYGONAL_PREV) readonly buffer LightSourcesPolygonalPrev_BT
{
    ShLightPolygonal lightSourcesPolygonal_Prev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_SPH_MATCH_PREV) readonly buffer LightSourcesSphMatchPrev_BT
{
    uint lightSourcesSphMatchPrev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_POLY_MATCH_PREV) readonly buffer LightSourcesPolyMatchPrev_BT
{
    uint lightSourcesPolyMatchPrev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_PLAIN_LIGHT_LIST_POLY) readonly buffer PlainLightListPoly_BT
{
    uint plainLightList_Poly[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_SECTOR_TO_LIGHT_LIST_REGION_POLY) readonly buffer SectorToLightListRegionPoly_BT
{
    // to get light list region for a sector,
    // [sectorArrayIndex * 2 + 0] is the start index in 'plainLightList'
    // [sectorArrayIndex * 2 + 1] is the end (excluding) index in 'plainLightList'
    uint sectorToLightListRegion_StartEnd_Poly[];
};
layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_PLAIN_LIGHT_LIST_SPH) readonly buffer PlainLightListSph_BT
{
    uint plainLightList_Sph[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_SECTOR_TO_LIGHT_LIST_REGION_SPH) readonly buffer SectorToLightListRegionSph_BT
{
    uint sectorToLightListRegion_StartEnd_Sph[];
};
#endif




#define CHECKERBOARD_SEPARATOR_DIVISOR 2

#ifdef DESC_SET_GLOBAL_UNIFORM
#ifdef DESC_SET_FRAMEBUFFERS
vec2 getPrevScreenPos(const vec2 motionCurToPrev, const ivec2 pix)
{
    const vec2 screenSize = vec2(globalUniform.renderWidth / float(CHECKERBOARD_SEPARATOR_DIVISOR), globalUniform.renderHeight);
    const vec2 invScreenSize = vec2(1.0 / screenSize.x, 1.0 / screenSize.y);
   
    return ((vec2(pix) + vec2(0.5)) * invScreenSize + motionCurToPrev) * screenSize;
}

vec2 getPrevScreenPos(sampler2D motionSampler, const ivec2 pix)
{
    return getPrevScreenPos(texelFetch(motionSampler, pix, 0).rg, pix);
}

/*
vec2 getCurScreenPos(sampler2D motionSampler, const ivec2 prevPix)
{
    const vec2 motionCurToPrev = texelFetch(motionSampler, prevPix, 0).rg;

    const vec2 screenSize = vec2(globalUniform.renderWidth, globalUniform.renderHeight);
    const vec2 invScreenSize = vec2(1.0 / float(globalUniform.renderWidth), 1.0 / float(globalUniform.renderHeight));
    
    return ((vec2(prevPix) + vec2(0.5)) * invScreenSize - motionCurToPrev) * screenSize;
}
*/

ivec2 getPrevFramePix(sampler2D motionSampler, const ivec2 curFramePix)
{
    return ivec2(floor(getPrevScreenPos(motionSampler, curFramePix) - 0.5));
}
#endif // DESC_SET_FRAMEBUFFERS
#endif // DESC_SET_GLOBAL_UNIFORM


#ifdef DESC_SET_GLOBAL_UNIFORM
    #define CHECKERBOARD_FULL_WIDTH globalUniform.renderWidth
    #define CHECKERBOARD_FULL_HEIGHT globalUniform.renderHeight
#endif // DESC_SET_GLOBAL_UNIFORM

#ifdef CHECKERBOARD_FULL_WIDTH
#ifdef CHECKERBOARD_FULL_HEIGHT
int getCheckerboardSeparatorX()
{
    return int(CHECKERBOARD_FULL_WIDTH) / CHECKERBOARD_SEPARATOR_DIVISOR;
}

int isRegularPixOdd(const ivec2 pix)
{
    return (pix.x + pix.y % 2) % 2;
}

int isCheckerboardPixOdd(const ivec2 checkerboardPix)
{
    return int(checkerboardPix.x >= getCheckerboardSeparatorX());
}

ivec2 getCheckerboardPix(const ivec2 pix)
{
    const int isOdd = isRegularPixOdd(pix);

    return ivec2(
        isOdd * getCheckerboardSeparatorX() + pix.x / 2,
        pix.y
    );
}

ivec2 getRegularPixFromCheckerboardPix(const ivec2 checkerboardPix)
{
    const int sep = getCheckerboardSeparatorX();
    const int isOdd = int(checkerboardPix.x >= sep);

    int x = checkerboardPix.x - isOdd * sep;

    return ivec2(
        x * 2 + (isOdd + checkerboardPix.y) % 2,
        checkerboardPix.y 
    );
}

// Render area for pixel, considering the checkerboard separator
ivec3 getCheckerboardedRenderArea(const ivec2 checkerboardPix)
{
    const int sep = getCheckerboardSeparatorX();
    const int isOdd = isCheckerboardPixOdd(checkerboardPix);

    return ivec3(
        // left bound
        (isOdd + 0) * sep,
        // right bound
        (isOdd + 1) * sep,
        CHECKERBOARD_FULL_HEIGHT
    );
}
#endif // CHECKERBOARD_FULL_HEIGHT
#endif // CHECKERBOARD_FULL_WIDTH

bool testPixInRenderArea(const ivec2 pix, const ivec3 renderArea)
{
    return 
        pix.y >= 0              && pix.y < renderArea[2] &&
        pix.x >= renderArea[0]  && pix.x < renderArea[1];
}

bool testReprojectedDepth(float z, float zPrev, float zMotion)
{
    return abs(z - zPrev + zMotion) / abs(z) < 0.1;
}

bool testReprojectedNormal(const vec3 n, const vec3 nPrev)
{
    return dot(n, nPrev) > 0.95;
}

bool testReprojectedNormalEnc(const vec3 n, const uint encodedNPrev)
{
    return testReprojectedNormal(n, decodeNormal(encodedNPrev));
}

float getAntilagAlpha(const float gradSample, const float normFactor)
{
    const float lambda = normFactor > 0.0001 ? 
        clamp(abs(gradSample) / normFactor, 0.0, 1.0) :
        0.0;

    return clamp(lambda, 0.0, 1.0);
}



#ifdef DESC_SET_FRAMEBUFFERS
#include "SphericalHarmonics.h"

#define SH_COMPRESSION_MULTIPLIER 1000 

SH texelFetchSH(sampler2D samplerIndirR, sampler2D samplerIndirG, sampler2D samplerIndirB, ivec2 pix)
{
    SH sh;
    sh.r = texelFetch(samplerIndirR, pix, 0) / SH_COMPRESSION_MULTIPLIER;
    sh.g = texelFetch(samplerIndirG, pix, 0) / SH_COMPRESSION_MULTIPLIER;
    sh.b = texelFetch(samplerIndirB, pix, 0) / SH_COMPRESSION_MULTIPLIER;

    return sh;
}

SH texelFetchUnfilteredIndirectSH(ivec2 pix)
{
    return texelFetchSH(
        framebufUnfilteredIndirectSH_R_Sampler,
        framebufUnfilteredIndirectSH_G_Sampler,
        framebufUnfilteredIndirectSH_B_Sampler,
        pix);
}

SH texelFetchIndirAccumSH(ivec2 pix)
{
    return texelFetchSH(
        framebufIndirAccumSH_R_Sampler,
        framebufIndirAccumSH_G_Sampler, 
        framebufIndirAccumSH_B_Sampler, 
        pix);
}

SH texelFetchIndirAccumSH_Prev(ivec2 pix)
{
    return texelFetchSH(
        framebufIndirAccumSH_R_Prev_Sampler,
        framebufIndirAccumSH_G_Prev_Sampler,
        framebufIndirAccumSH_B_Prev_Sampler,
        pix);
}

SH imageLoadUnfilteredIndirectSH(ivec2 pix)
{
    SH sh;
    sh.r = imageLoad(framebufUnfilteredIndirectSH_R, pix) / SH_COMPRESSION_MULTIPLIER;
    sh.g = imageLoad(framebufUnfilteredIndirectSH_G, pix) / SH_COMPRESSION_MULTIPLIER;
    sh.b = imageLoad(framebufUnfilteredIndirectSH_B, pix) / SH_COMPRESSION_MULTIPLIER;

    return sh;
}

void imageStoreUnfilteredIndirectSH(ivec2 pix, const SH sh)
{
    imageStore(framebufUnfilteredIndirectSH_R, pix, sh.r * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufUnfilteredIndirectSH_G, pix, sh.g * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufUnfilteredIndirectSH_B, pix, sh.b * SH_COMPRESSION_MULTIPLIER);
}

void imageStoreIndirAccumSH(ivec2 pix, const SH sh)
{
    imageStore(framebufIndirAccumSH_R, pix, sh.r * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirAccumSH_G, pix, sh.g * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirAccumSH_B, pix, sh.b * SH_COMPRESSION_MULTIPLIER);
}

void imageStoreIndirPingSH(ivec2 pix, const SH sh)
{
    imageStore(framebufIndirPingSH_R, pix, sh.r * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirPingSH_G, pix, sh.g * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirPingSH_B, pix, sh.b * SH_COMPRESSION_MULTIPLIER);
}

void imageStoreIndirPongSH(ivec2 pix, const SH sh)
{
    imageStore(framebufIndirPongSH_R, pix, sh.r * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirPongSH_G, pix, sh.g * SH_COMPRESSION_MULTIPLIER);
    imageStore(framebufIndirPongSH_B, pix, sh.b * SH_COMPRESSION_MULTIPLIER);
}

vec3 texelFetchNormal(const ivec2 pix)
{
    return decodeNormal(texelFetch(framebufNormal_Sampler, pix, 0).r);
} 

vec3 texelFetchNormal_Prev(const ivec2 pix)
{
    return decodeNormal(texelFetch(framebufNormal_Prev_Sampler, pix, 0).r);
}

vec3 texelFetchNormalGeometry(const ivec2 pix)
{
    return decodeNormal(texelFetch(framebufNormalGeometry_Sampler, pix, 0).r);
}

vec3 texelFetchNormalGeometry_Prev(const ivec2 pix)
{
    return decodeNormal(texelFetch(framebufNormalGeometry_Prev_Sampler, pix, 0).r);
}

uvec4 textureGatherEncNormalGeometry_Prev(const vec2 uv)
{
    // get R components of 4 texels 
    return textureGather(framebufNormalGeometry_Prev_Sampler, uv, 0);
}

uint texelFetchEncNormalGeometry(const ivec2 pix)
{
    // fetch encoded normal
    return texelFetch(framebufNormalGeometry_Sampler, pix, 0).r;
}

void imageStoreNormal(const ivec2 pix, const vec3 normal)
{
    imageStore(framebufNormal, pix, uvec4(encodeNormal(normal)));
}

void imageStoreNormalGeometry(const ivec2 pix, const vec3 normal)
{
    imageStore(framebufNormalGeometry, pix, uvec4(encodeNormal(normal)));
}


#ifdef CHECKERBOARD_FULL_WIDTH
#ifdef CHECKERBOARD_FULL_HEIGHT

// framebufAlbedo ALWAYS uses regular layout because of the sky rasterization pass  

void imageStoreAlbedoSurface(const ivec2 pix, const vec3 surfaceAlbedo, float screenEmission)
{
    imageStore(framebufAlbedo, getRegularPixFromCheckerboardPix(pix), vec4(surfaceAlbedo, max(0.0, screenEmission)));
}

void imageStoreAlbedoSky(const ivec2 pix, const vec3 skyAlbedo)
{
    imageStore(framebufAlbedo, getRegularPixFromCheckerboardPix(pix), vec4(skyAlbedo, -1.0));
}

vec4 texelFetchAlbedo(const ivec2 pix)
{
    return texelFetch(framebufAlbedo_Sampler, getRegularPixFromCheckerboardPix(pix), 0);
}

#endif // CHECKERBOARD_FULL_HEIGHT
#endif // CHECKERBOARD_FULL_WIDTH

vec4 textureLodAlbedo(const vec2 uv)
{
    // framebufAlbedo has nearest filtering, so values won't be interpolated
    return textureLod(framebufAlbedo_Sampler, uv, 0);
}

float getScreenEmissionFromAlbedo4(const vec4 albedo)
{
    return max(0.0, albedo.a);
}

bool isSky(const vec4 albedo)
{
    return albedo.a < 0.0;
}

#endif // DESC_SET_FRAMEBUFFERS
