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

#define UINT32_MAX  0xFFFFFFFF

vec4 unpackLittleEndianUintColor(uint c)
{
    return vec4(
         (c & 0x000000FF)        / 255.0,
        ((c & 0x0000FF00) >> 8)  / 255.0,
        ((c & 0x00FF0000) >> 16) / 255.0,
        ((c & 0xFF000000) >> 24) / 255.0
    );
}

#ifdef DESC_SET_GLOBAL_UNIFORM
layout(
    set = DESC_SET_GLOBAL_UNIFORM,
    binding = BINDING_GLOBAL_UNIFORM)
    readonly uniform GlobalUniform_BT
{
    ShGlobalUniform globalUniform;
};

#ifdef DESC_SET_VERTEX_DATA
layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_VERTEX_BUFFER_STATIC)
    #ifndef VERTEX_BUFFER_WRITEABLE
    readonly 
    #endif
    buffer VertexBufferStatic_BT
{
    ShVertexBufferStatic staticVertices;
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_VERTEX_BUFFER_DYNAMIC)
    #ifndef VERTEX_BUFFER_WRITEABLE
    readonly 
    #endif
    buffer VertexBufferDynamic_BT
{
    ShVertexBufferDynamic dynamicVertices;
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_INDEX_BUFFER_STATIC)
    readonly 
    buffer IndexBufferStatic_BT
{
    uint staticIndices[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_INDEX_BUFFER_DYNAMIC)
    readonly 
    buffer IndexBufferDynamic_BT
{
    uint dynamicIndices[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_GEOMETRY_INSTANCES_STATIC)
    readonly 
    buffer GeometryInstancesStatic_BT
{
    ShGeometryInstance geometryInstancesStatic[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_GEOMETRY_INSTANCES_DYNAMIC)
    readonly 
    buffer GeometryInstancesDynamic_BT
{
    ShGeometryInstance geometryInstancesDynamic[];
};

vec3 getStaticVerticesPositions(uint index)
{
    return vec3(
        staticVertices.positions[index * globalUniform.positionsStride + 0],
        staticVertices.positions[index * globalUniform.positionsStride + 1],
        staticVertices.positions[index * globalUniform.positionsStride + 2]);
}

vec3 getStaticVerticesNormals(uint index)
{
    return vec3(
        staticVertices.normals[index * globalUniform.normalsStride + 0],
        staticVertices.normals[index * globalUniform.normalsStride + 1],
        staticVertices.normals[index * globalUniform.normalsStride + 2]);
}

vec2 getStaticVerticesTexCoords(uint index)
{
    return vec2(
        staticVertices.texCoords[index * globalUniform.texCoordsStride + 0],
        staticVertices.texCoords[index * globalUniform.texCoordsStride + 1]);
}

vec2 getStaticVerticesTexCoordsLayer1(uint index)
{
    return vec2(
        staticVertices.texCoordsLayer1[index * globalUniform.texCoordsStride + 0],
        staticVertices.texCoordsLayer1[index * globalUniform.texCoordsStride + 1]);
}

vec2 getStaticVerticesTexCoordsLayer2(uint index)
{
    return vec2(
        staticVertices.texCoordsLayer2[index * globalUniform.texCoordsStride + 0],
        staticVertices.texCoordsLayer2[index * globalUniform.texCoordsStride + 1]);
}

vec3 getDynamicVerticesPositions(uint index)
{
    return vec3(
        dynamicVertices.positions[index * globalUniform.positionsStride + 0],
        dynamicVertices.positions[index * globalUniform.positionsStride + 1],
        dynamicVertices.positions[index * globalUniform.positionsStride + 2]);
}

vec3 getDynamicVerticesNormals(uint index)
{
    return vec3(
        dynamicVertices.normals[index * globalUniform.normalsStride + 0],
        dynamicVertices.normals[index * globalUniform.normalsStride + 1],
        dynamicVertices.normals[index * globalUniform.normalsStride + 2]);
}

vec2 getDynamicVerticesTexCoords(uint index)
{
    return vec2(
        dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 0],
        dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 1]);
}

#ifdef VERTEX_BUFFER_WRITEABLE
void setStaticVerticesPositions(uint index, vec3 value)
{
    staticVertices.positions[index * globalUniform.positionsStride + 0] = value[0];
    staticVertices.positions[index * globalUniform.positionsStride + 1] = value[1];
    staticVertices.positions[index * globalUniform.positionsStride + 2] = value[2];
}

void setStaticVerticesNormals(uint index, vec3 value)
{
    staticVertices.normals[index * globalUniform.normalsStride + 0] = value[0];
    staticVertices.normals[index * globalUniform.normalsStride + 1] = value[1];
    staticVertices.normals[index * globalUniform.normalsStride + 2] = value[2];
}

void setStaticVerticesTexCoords(uint index, vec2 value)
{
    staticVertices.texCoords[index * globalUniform.texCoordsStride + 0] = value[0];
    staticVertices.texCoords[index * globalUniform.texCoordsStride + 1] = value[1];
}

void setStaticVerticesTexCoordsLayer1(uint index, vec2 value)
{
    staticVertices.texCoordsLayer1[index * globalUniform.texCoordsStride + 0] = value[0];
    staticVertices.texCoordsLayer1[index * globalUniform.texCoordsStride + 1] = value[1];
}

void setStaticVerticesTexCoordsLayer2(uint index, vec2 value)
{
    staticVertices.texCoordsLayer2[index * globalUniform.texCoordsStride + 0] = value[0];
    staticVertices.texCoordsLayer2[index * globalUniform.texCoordsStride + 1] = value[1];
}

void setDynamicVerticesPositions(uint index, vec3 value)
{
    dynamicVertices.positions[index * globalUniform.positionsStride + 0] = value[0];
    dynamicVertices.positions[index * globalUniform.positionsStride + 1] = value[1];
    dynamicVertices.positions[index * globalUniform.positionsStride + 2] = value[2];
}

void setDynamicVerticesNormals(uint index, vec3 value)
{
    dynamicVertices.normals[index * globalUniform.normalsStride + 0] = value[0];
    dynamicVertices.normals[index * globalUniform.normalsStride + 1] = value[1];
    dynamicVertices.normals[index * globalUniform.normalsStride + 2] = value[2];
}

void setDynamicVerticesTexCoords(uint index, vec2 value)
{
    dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 0] = value[0];
    dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 1] = value[1];
}
#endif // VERTEX_BUFFER_WRITEABLE

uvec3 getVertIndicesStatic(uint baseVertexIndex, uint baseIndexIndex, uint primitiveId)
{
    // if to use indices
    if (baseIndexIndex != UINT32_MAX)
    {
        return uvec3(
            baseVertexIndex + staticIndices[baseIndexIndex + primitiveId * 3 + 0],
            baseVertexIndex + staticIndices[baseIndexIndex + primitiveId * 3 + 1],
            baseVertexIndex + staticIndices[baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return uvec3(
            baseVertexIndex + primitiveId * 3 + 0,
            baseVertexIndex + primitiveId * 3 + 1,
            baseVertexIndex + primitiveId * 3 + 2);
    }
}

uvec3 getVertIndicesDynamic(uint baseVertexIndex, uint baseIndexIndex, uint primitiveId)
{
    // if to use indices
    if (baseIndexIndex != UINT32_MAX)
    {
        return uvec3(
            baseVertexIndex + dynamicIndices[baseIndexIndex + primitiveId * 3 + 0],
            baseVertexIndex + dynamicIndices[baseIndexIndex + primitiveId * 3 + 1],
            baseVertexIndex + dynamicIndices[baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return uvec3(
            baseVertexIndex + primitiveId * 3 + 0,
            baseVertexIndex + primitiveId * 3 + 1,
            baseVertexIndex + primitiveId * 3 + 2);
    }
}

ShTriangle getTriangleStatic(uvec3 vertIndices)
{
    ShTriangle tr;

    tr.positions[0] = getStaticVerticesPositions(vertIndices[0]);
    tr.positions[1] = getStaticVerticesPositions(vertIndices[1]);
    tr.positions[2] = getStaticVerticesPositions(vertIndices[2]);

    tr.normals[0] = getStaticVerticesNormals(vertIndices[0]);
    tr.normals[1] = getStaticVerticesNormals(vertIndices[1]);
    tr.normals[2] = getStaticVerticesNormals(vertIndices[2]);

    tr.layerTexCoord[0][0] = getStaticVerticesTexCoords(vertIndices[0]);
    tr.layerTexCoord[0][1] = getStaticVerticesTexCoords(vertIndices[1]);
    tr.layerTexCoord[0][2] = getStaticVerticesTexCoords(vertIndices[2]);

    tr.layerTexCoord[1][0] = getStaticVerticesTexCoordsLayer1(vertIndices[0]);
    tr.layerTexCoord[1][1] = getStaticVerticesTexCoordsLayer1(vertIndices[1]);
    tr.layerTexCoord[1][2] = getStaticVerticesTexCoordsLayer1(vertIndices[2]);

    tr.layerTexCoord[2][0] = getStaticVerticesTexCoordsLayer2(vertIndices[0]);
    tr.layerTexCoord[2][1] = getStaticVerticesTexCoordsLayer2(vertIndices[1]);
    tr.layerTexCoord[2][2] = getStaticVerticesTexCoordsLayer2(vertIndices[2]);

    // TODO
    //tr.tangent = getStaticVerticesTangent(vertIndices[0] / 3);

    return tr;
}

ShTriangle getTriangleDynamic(uvec3 vertIndices)
{
    ShTriangle tr;

    tr.positions[0] = getDynamicVerticesPositions(vertIndices[0]);
    tr.positions[1] = getDynamicVerticesPositions(vertIndices[1]);
    tr.positions[2] = getDynamicVerticesPositions(vertIndices[2]);

    tr.normals[0] = getDynamicVerticesNormals(vertIndices[0]);
    tr.normals[1] = getDynamicVerticesNormals(vertIndices[1]);
    tr.normals[2] = getDynamicVerticesNormals(vertIndices[2]);

    tr.layerTexCoord[0][0] = getDynamicVerticesTexCoords(vertIndices[0]);
    tr.layerTexCoord[0][1] = getDynamicVerticesTexCoords(vertIndices[1]);
    tr.layerTexCoord[0][2] = getDynamicVerticesTexCoords(vertIndices[2]);

    // TODO
    //tr.tangent = getDynamicVerticesTangent(vertIndices[0] / 3);

    return tr;
}

// localGeometryIndex is index of geometry in pGeometries in BLAS
// primitiveId is index of a triangle
ShTriangle getTriangle(int instanceID, int instanceCustomIndex, int localGeometryIndex, int primitiveId)
{
    bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    
    ShGeometryInstance inst;
    ShTriangle tr;

    int geometryIndex = globalUniform.instanceGeomInfoOffset[instanceID].x + localGeometryIndex;

    if (isDynamic)
    {
        // get info about geometry from pGeometries in one of BLAS
        inst = geometryInstancesDynamic[geometryIndex];
        uvec3 vertIndices = getVertIndicesDynamic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        tr = getTriangleDynamic(vertIndices);

        // only one material for dynamic geometry
        tr.materials[0] = uvec3(inst.materials[0]);
        tr.materials[1] = uvec3(MATERIAL_NO_TEXTURE);
        tr.materials[2] = uvec3(MATERIAL_NO_TEXTURE);
    }
    else
    {
        inst = geometryInstancesStatic[geometryIndex];
        uvec3 vertIndices = getVertIndicesStatic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        tr = getTriangleStatic(vertIndices);

        tr.materials[0] = uvec3(inst.materials[0]);
        tr.materials[1] = uvec3(inst.materials[1]);
        tr.materials[2] = uvec3(inst.materials[2]);
    }

    tr.geomColor = inst.color;
    tr.geomRoughness = inst.defaultRoughness;
    tr.geomMetallicity = inst.defaultMetallicity;
    tr.geomEmission = tr.geomColor.rgb * inst.defaultEmission;

    return tr;
}

mat4 getModelMatrix(bool isDynamic, int geometryIndex)
{
    if (isDynamic)
    {
        return geometryInstancesDynamic[geometryIndex].model;
    }
    else
    {
        return geometryInstancesStatic[geometryIndex].model;
    }
}

mat4 getModelMatrix(int instanceID, int instanceCustomIndex, int localGeometryIndex)
{
    bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    int geometryIndex = globalUniform.instanceGeomInfoOffset[instanceID].x + localGeometryIndex;

    return getModelMatrix(isDynamic, geometryIndex);
}
#endif // DESC_SET_VERTEX_DATA
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
#endif // DESC_SET_TEXTURES



vec4 blendUnder(vec4 src, vec4 dst)
{
    // dst is under src
    return dst * (vec4(1.0) - src);
    //return src * src.a + dst * (vec4(1.0) - src);
}

vec4 blendAdditive(vec4 src, vec4 dst)
{
    return src * src.a + dst;
}

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
ShHitInfo getHitInfo(ShPayload pl)
{
    ShHitInfo h;

    int instanceId, instCustomIndex;
    int geomIndex, primIndex;

    unpackInstanceIdAndCustomIndex(pl.instIdAndIndex, instanceId, instCustomIndex);
    unpackGeometryAndPrimitiveIndex(pl.geomAndPrimIndex, geomIndex, primIndex);

    ShTriangle tr = getTriangle(instanceId, instCustomIndex, geomIndex, primIndex);
    mat4 model = getModelMatrix(instanceId, instCustomIndex, geomIndex);

    vec2 inBaryCoords = pl.baryCoords;
    vec3 baryCoords = vec3(1.0f - inBaryCoords.x - inBaryCoords.y, inBaryCoords.x, inBaryCoords.y);
    
    
    vec2 texCoords[] = 
    {
        tr.layerTexCoord[0] * baryCoords,
        tr.layerTexCoord[1] * baryCoords,
        tr.layerTexCoord[2] * baryCoords
    };
    

    h.albedo = tr.geomColor.rgb;

    if (tr.materials[0][MATERIAL_ALBEDO_ALPHA_INDEX] != MATERIAL_NO_TEXTURE)
    {
        h.albedo *= getTextureSample(tr.materials[0][MATERIAL_ALBEDO_ALPHA_INDEX], texCoords[0]).rgb;
    }
    if (tr.materials[1][MATERIAL_ALBEDO_ALPHA_INDEX] != MATERIAL_NO_TEXTURE)
    {
        h.albedo *= getTextureSample(tr.materials[1][MATERIAL_ALBEDO_ALPHA_INDEX], texCoords[1]).rgb;
    }   
    if (tr.materials[2][MATERIAL_ALBEDO_ALPHA_INDEX] != MATERIAL_NO_TEXTURE)
    {
        h.albedo *= getTextureSample(tr.materials[2][MATERIAL_ALBEDO_ALPHA_INDEX], texCoords[2]).rgb;
    }
    

    // convert normals to world space
    tr.normals[0] = vec3(model * vec4(tr.normals[0], 0.0));
    tr.normals[1] = vec3(model * vec4(tr.normals[1], 0.0));
    tr.normals[2] = vec3(model * vec4(tr.normals[2], 0.0));

    h.normalGeom = normalize(tr.normals * baryCoords);


    if (tr.materials[0][MATERIAL_NORMAL_METALLIC_INDEX] != MATERIAL_NO_TEXTURE)
    {
        vec4 nm = getTextureSample(tr.materials[0][MATERIAL_NORMAL_METALLIC_INDEX], texCoords[0]);
        h.metallic = nm.a;

        // TODO: normal maps, tangents
        h.normal = h.normalGeom;
    }
    else
    {
        h.normal = h.normalGeom;
        h.metallic = tr.geomMetallicity;
    }
    

    if (tr.materials[0][MATERIAL_EMISSION_ROUGHNESS_INDEX] != MATERIAL_NO_TEXTURE)
    {
        vec4 er = getTextureSample(tr.materials[0][MATERIAL_EMISSION_ROUGHNESS_INDEX], texCoords[0]);
        h.emission = er.rgb;
        h.roughness = er.a;
    }
    else
    {
        h.emission = tr.geomEmission;
        h.roughness = tr.geomRoughness;
    }

    h.hitDistance = pl.clsHitDistance;

    h.instCustomIndex = instCustomIndex;

    return h;
}
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


float getLuminance(vec3 c)
{
    return 0.2125 * c.r + 0.7154 * c.g + 0.0721 * c.b;
}