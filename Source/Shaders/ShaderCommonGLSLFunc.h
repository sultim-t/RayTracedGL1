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
// * DESC_SET_LENS_FLARES
// * DESC_SET_DECALS
// * DESC_SET_RESTIR_INDIRECT
// * DESC_SET_VOLUMETRIC



#define FAKE_ROUGH_SPECULAR_THRESHOLD 0.5
#define FAKE_ROUGH_SPECULAR_LENGTH 0.25

#define SHIPPING_HACK 1
#define ILLUMINATION_VOLUME 0



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
layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES) readonly buffer LightSources_BT
{
    ShLightEncoded lightSources[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_PREV) readonly buffer LightSourcesPrev_BT
{
    ShLightEncoded lightSources_Prev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_INDEX_PREV_TO_CUR) readonly buffer LightSourcesIndexPrevToCur_BT
{
    uint lightSources_Index_PrevToCur[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_INDEX_CUR_TO_PREV) readonly buffer LightSourcesIndexCurToPrev_BT
{
    uint lightSources_Index_CurToPrev[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_INITIAL_LIGHTS_GRID) 
#ifndef LIGHT_GRID_WRITE
readonly
#endif
buffer InitialLightsGrid_BT
{
    ShLightInCell initialLightsGrid[];
};

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_INITIAL_LIGHTS_GRID_PREV) readonly buffer InitialLightsGridPrev_BT
{
    ShLightInCell initialLightsGrid_Prev[];
};
#endif



#ifdef DESC_SET_RESTIR_INDIRECT
layout(set = DESC_SET_RESTIR_INDIRECT, binding = BINDING_RESTIR_INDIRECT_INITIAL_SAMPLES) buffer RestirIndirectInitialSamples_BT
{
    uint g_restirIndirectInitialSamples[];
};

layout(set = DESC_SET_RESTIR_INDIRECT, binding = BINDING_RESTIR_INDIRECT_RESERVOIRS) buffer RestirIndirectReservoirs_BT
{
    uint g_restirIndirectReservoirs[];
};

layout(set = DESC_SET_RESTIR_INDIRECT, binding = BINDING_RESTIR_INDIRECT_RESERVOIRS_PREV) buffer RestirIndirectReservoirs_Prev_BT
{
    uint g_restirIndirectReservoirs_Prev[];
};
#endif



#ifdef DESC_SET_VOLUMETRIC
layout(set = DESC_SET_VOLUMETRIC, binding = BINDING_VOLUMETRIC_STORAGE, rgba16f) 
uniform image3D g_volumetric;

layout(set = DESC_SET_VOLUMETRIC, binding = BINDING_VOLUMETRIC_SAMPLER) 
uniform sampler3D g_volumetric_Sampler;

layout(set = DESC_SET_VOLUMETRIC, binding = BINDING_VOLUMETRIC_SAMPLER_PREV) 
uniform sampler3D g_volumetric_Sampler_Prev;

layout(set = DESC_SET_VOLUMETRIC, binding = BINDING_VOLUMETRIC_ILLUMINATION, r11f_g11f_b10f) 
uniform image3D g_illuminationVolume;

layout(set = DESC_SET_VOLUMETRIC, binding = BINDING_VOLUMETRIC_ILLUMINATION_SAMPLER) 
uniform sampler3D g_illuminationVolume_Sampler;
#endif



#ifdef DESC_SET_LENS_FLARES
layout(set = DESC_SET_LENS_FLARES, binding = BINDING_LENS_FLARES_CULLING_INPUT) readonly buffer LensFlareCullingInput_BT
{
    ShIndirectDrawCommand lensFlareCullingInput[];
};

layout(set = DESC_SET_LENS_FLARES, binding = BINDING_LENS_FLARES_DRAW_CMDS) buffer LensFlareDrawCmds_BT
{
    ShIndirectDrawCommand lensFlareDrawCmds[LENS_FLARES_MAX_DRAW_CMD_COUNT];
    uint lensFlareDrawCmdsCount;
};
#endif // DESC_SET_LENS_FLARES



#ifdef DESC_SET_DECALS
layout(set = DESC_SET_DECALS, binding = BINDING_DECAL_INSTANCES) buffer DecalInstances_BT
{
    ShDecalInstance decalInstances[];
};
#endif // DESC_SET_DECALS



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

uint texelFetchEncNormal(const ivec2 pix)
{
    // fetch encoded normal
    return texelFetch(framebufNormal_Sampler, pix, 0).r;
}

uint texelFetchEncNormalGeometry(const ivec2 pix)
{
    // fetch encoded geometry normal
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

bool isSkyPix(const ivec2 pix)
{
    return texelFetch(framebufIsSky_Sampler, pix, 0).r != 0;
}

// t == 0
bool wasOnlyPrimary( float t )
{
    return abs( t ) < 0.5;
}

// t == -1: was refl/refr without a split, e.g. portal/mirror
bool wasWithoutSplit( float t )
{
    return t < 0.5;
}

// t == 1: was refl/refr with a split, e.g. water/glass
bool wasSplit( float t )
{
    return t > 0.5;
}

bool needResolveCheckerboard( const ivec2 checkerboardPix )
{
    float t = texelFetch( framebufThroughput_Sampler, checkerboardPix, 0 ).a;
    return wasSplit( t );
}
#endif // DESC_SET_FRAMEBUFFERS



vec4 unpackEmissiveFactorAndStrength( uint packed )
{
    vec4 fs = unpackUintColor( packed );
    // from normalized
    fs.a *= 255;
    return fs;
}



#ifdef DESC_SET_GLOBAL_UNIFORM
vec3 getRayDir( vec2 inUV )
{
    inUV = inUV * 2.0 - 1.0;

    vec4 target   = globalUniform.invProjection * vec4( inUV.x, inUV.y, 1, 1 );
    vec3 localDir = abs( target.w ) < 0.001 ? target.xyz : target.xyz / target.w;

    vec4 rayDir = globalUniform.invView * vec4( normalize( localDir ), 0 );

    return rayDir.xyz;
}

vec2 getPixelUVWithJitter( const ivec2 pix )
{
    const vec2 pixelCenter = vec2( pix ) + vec2( 0.5 );
    const vec2 jitter      = vec2( globalUniform.jitterX, globalUniform.jitterY );

    return ( pixelCenter + jitter ) / vec2( globalUniform.renderWidth, globalUniform.renderHeight );
}

vec3 getRayDirAX( vec2 inUV )
{
    const float AX = 1.0 / globalUniform.renderWidth;
    return getRayDir( inUV + vec2( AX, 0 ) );
}

vec3 getRayDirAY( vec2 inUV )
{
    const float AY = 1.0 / globalUniform.renderHeight;
    return getRayDir( inUV + vec2( 0, AY ) );
}

bool classicShading( ivec2 regularPix )
{
    return globalUniform.lightmapEnable != 0 ? regularPix.x < globalUniform.renderWidth / 2 : false;
}
#endif // DESC_SET_GLOBAL_UNIFORM