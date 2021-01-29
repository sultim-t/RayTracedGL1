#include "ShaderCommonGLSL.h"

#extension GL_EXT_nonuniform_qualifier : enable

// Functions to access RTGL data.
// Available defines:
// * DESC_SET_GLOBAL_UNIFORM    -- to access global uniform buffer
// * DESC_SET_VERTEX_DATA       -- to access geometry data. DESC_SET_GLOBAL_UNIFORM must be defined
// * DESC_SET_TEXTURES          -- to access textures by index

#define UINT32_MAX 0xFFFFFFFF

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

    tr.texCoords[0] = getStaticVerticesTexCoords(vertIndices[0]);
    tr.texCoords[1] = getStaticVerticesTexCoords(vertIndices[1]);
    tr.texCoords[2] = getStaticVerticesTexCoords(vertIndices[2]);

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

    tr.texCoords[0] = getDynamicVerticesTexCoords(vertIndices[0]);
    tr.texCoords[1] = getDynamicVerticesTexCoords(vertIndices[1]);
    tr.texCoords[2] = getDynamicVerticesTexCoords(vertIndices[2]);

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
    }
    else
    {
        inst = geometryInstancesStatic[geometryIndex];
        uvec3 vertIndices = getVertIndicesStatic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        tr = getTriangleStatic(vertIndices);
    }

    tr.materials[0] = uvec3(inst.materials[0]);
    tr.materials[1] = uvec3(inst.materials[1]);
    tr.materials[2] = uvec3(inst.materials[2]);

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

vec4 getTextureSampleSafe(uint textureIndex, vec2 texCoord)
{
    if (textureIndex != 0)
    {
        return texture(globalTextures[nonuniformEXT(textureIndex)], texCoord);
    }
    else
    {
        return vec4(1.0, 1.0, 1.0, 1.0);
    }
}
#endif