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

#include "ShaderCommonGLSL.h"
#include "BRDF.h"
#include "Utils.h"

#extension GL_EXT_nonuniform_qualifier : enable

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



#ifdef DESC_SET_GLOBAL_UNIFORM
layout(
    set = DESC_SET_GLOBAL_UNIFORM,
    binding = BINDING_GLOBAL_UNIFORM)
    readonly uniform GlobalUniform_BT
{
    ShGlobalUniform globalUniform;
};

#ifdef DESC_SET_VERTEX_DATA
    #include "VertexData.inl"
#endif

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

vec4 getTextureSample(uint textureIndex, vec2 texCoord)
{
    return texture(globalTextures[nonuniformEXT(textureIndex)], texCoord);
}

vec4 getTextureSampleGrad(uint textureIndex, vec2 texCoord, vec2 dPdx, vec2 dPdy)
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



#ifdef DESC_SET_VERTEX_DATA
#ifdef DESC_SET_GLOBAL_UNIFORM
#ifdef DESC_SET_TEXTURES

    #include "HitInfo.inl"

    #define TEXTURE_GRADIENTS
        #include "HitInfo.inl"
    #undef TEXTURE_GRADIENTS

#endif // DESC_SET_TEXTURES
#endif // DESC_SET_GLOBAL_UNIFORM
#endif // DESC_SET_VERTEX_DATA



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

layout(set = DESC_SET_LIGHT_SOURCES, binding = BINDING_LIGHT_SOURCES_DIRECTIONAL) readonly buffer LightSourcesDirectional_BT
{
    ShLightDirectional lightSourcesDirecitional[];
};
#endif