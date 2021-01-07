#include "ShaderCommonGLSL.h"

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

    tr.textureCoords[0] = getStaticVerticesTexCoords(vertIndices[0]);
    tr.textureCoords[1] = getStaticVerticesTexCoords(vertIndices[1]);
    tr.textureCoords[2] = getStaticVerticesTexCoords(vertIndices[2]);

    // TODO
    //tr.tangent = getStaticVerticesTangent(vertIndices[0] / 3);

    //tr.materialIds[0] = getStaticVerticesMaterialIds(vertIndices[0]);
    //tr.materialIds[1] = getStaticVerticesMaterialIds(vertIndices[1]);
    //tr.materialIds[2] = getStaticVerticesMaterialIds(vertIndices[2]);

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

    tr.textureCoords[0] = getDynamicVerticesTexCoords(vertIndices[0]);
    tr.textureCoords[1] = getDynamicVerticesTexCoords(vertIndices[1]);
    tr.textureCoords[2] = getDynamicVerticesTexCoords(vertIndices[2]);

    // TODO
    //tr.tangent = getDynamicVerticesTangent(vertIndices[0] / 3);

    //tr.materialIds[0] = getDynamicVerticesMaterialIds(vertIndices[0]);
    //tr.materialIds[1] = getDynamicVerticesMaterialIds(vertIndices[1]);
    //tr.materialIds[2] = getDynamicVerticesMaterialIds(vertIndices[2]);

    return tr;
}

// geometryIndex is index of geometry in pGeometries in BLAS
// primitiveId is index of a triangle
ShTriangle getTriangle(int instanceCustomIndex, int geometryIndex, int primitiveId)
{
    bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    
    if (isDynamic)
    {
        // get info about geometry from pGeometries in one of BLAS
        ShGeometryInstance inst = geometryInstancesDynamic[geometryIndex];
        uvec3 vertIndices = getVertIndicesDynamic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        return getTriangleDynamic(vertIndices);
    }
    else
    {
        ShGeometryInstance inst = geometryInstancesStatic[geometryIndex];
        uvec3 vertIndices = getVertIndicesStatic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        return getTriangleStatic(vertIndices);
    }
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

mat4 getModelMatrix(int instanceCustomIndex, int geometryIndex)
{
    bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    
    return getModelMatrix(isDynamic, geometryIndex);
}
#endif // DESC_SET_VERTEX_DATA
#endif // DESC_SET_GLOBAL_UNIFORM