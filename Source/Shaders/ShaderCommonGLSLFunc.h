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
    readonly buffer VertexBufferStatic_BT
{
    ShVertexBufferStatic staticVertices;
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_VERTEX_BUFFER_DYNAMIC)
    readonly buffer VertexBufferDynamic_BT
{
    ShVertexBufferDynamic dynamicVertices;
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_INDEX_BUFFER_STATIC)
    readonly buffer IndexBufferStatic_BT
{
    uint staticIndices[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_INDEX_BUFFER_DYNAMIC)
    readonly buffer IndexBufferDynamic_BT
{
    uint dynamicIndices[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_GEOMETRY_INSTANCES_STATIC)
    readonly buffer GeometryInstancesStatic_BT
{
    ShGeometryInstance geometryInstancesStatic[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_GEOMETRY_INSTANCES_DYNAMIC)
    readonly buffer GeometryInstancesDynamic_BT
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

// get info about geometry from pGeometries in one of BLAS
ShGeometryInstance getGeomInstanceStatic(int geometryIndex)
{
    ShGeometryInstance inst;
    inst.baseVertexIndex = geometryInstancesStatic[geometryIndex].baseVertexIndex;
    inst.baseIndexIndex = geometryInstancesStatic[geometryIndex].baseIndexIndex;
    inst.materialId0 = geometryInstancesStatic[geometryIndex].materialId0;
    inst.materialId1 = geometryInstancesStatic[geometryIndex].materialId1;
    inst.materialId2 = geometryInstancesStatic[geometryIndex].materialId2;

    return inst;
}
ShGeometryInstance getGeomInstanceDynamic(int geometryIndex)
{
    ShGeometryInstance inst;
    inst.baseVertexIndex = geometryInstancesDynamic[geometryIndex].baseVertexIndex;
    inst.baseIndexIndex = geometryInstancesDynamic[geometryIndex].baseIndexIndex;
    inst.materialId0 = geometryInstancesDynamic[geometryIndex].materialId0;
    inst.materialId1 = geometryInstancesDynamic[geometryIndex].materialId1;
    inst.materialId2 = geometryInstancesDynamic[geometryIndex].materialId2;

    return inst;
}

uvec3 getVertIndicesStatic(ShGeometryInstance inst, int primitiveId)
{
    // if to use indices
    if (inst.baseIndexIndex != UINT32_MAX)
    {
        return uvec3(
            inst.baseVertexIndex + staticIndices[inst.baseIndexIndex + primitiveId * 3 + 0],
            inst.baseVertexIndex + staticIndices[inst.baseIndexIndex + primitiveId * 3 + 1],
            inst.baseVertexIndex + staticIndices[inst.baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return uvec3(
            inst.baseVertexIndex + primitiveId * 3 + 0,
            inst.baseVertexIndex + primitiveId * 3 + 1,
            inst.baseVertexIndex + primitiveId * 3 + 2);
    }
}

uvec3 getVertIndicesDynamic(ShGeometryInstance inst, int primitiveId)
{
    // if to use indices
    if (inst.baseIndexIndex != UINT32_MAX)
    {
        return uvec3(
            inst.baseVertexIndex + dynamicIndices[inst.baseIndexIndex + primitiveId * 3 + 0],
            inst.baseVertexIndex + dynamicIndices[inst.baseIndexIndex + primitiveId * 3 + 1],
            inst.baseVertexIndex + dynamicIndices[inst.baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return uvec3(
            inst.baseVertexIndex + primitiveId * 3 + 0,
            inst.baseVertexIndex + primitiveId * 3 + 1,
            inst.baseVertexIndex + primitiveId * 3 + 2);
    }
}

ShTriangle getTriangleStatic(ShGeometryInstance inst, int primitiveId, uvec3 vertIndices)
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

    tr.tangent = vec3(0);

    tr.materialIds = uvec3(inst.materialId0, inst.materialId1, inst.materialId2);

    return tr;
}

ShTriangle getTriangleDynamic(ShGeometryInstance inst, int primitiveId, uvec3 vertIndices)
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

    tr.tangent = vec3(0);

    tr.materialIds = uvec3(inst.materialId0, inst.materialId1, inst.materialId2);

    return tr;
}

// geometryIndex is index of geometry in pGeometries in BLAS
// primitiveId is index of a triangle
ShTriangle getTriangle(int instanceCustomIndex, int geometryIndex, int primitiveId)
{
    bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    
    if (isDynamic)
    {
        ShGeometryInstance inst = getGeomInstanceDynamic(geometryIndex);
        uvec3 vertIndices = getVertIndicesDynamic(inst, primitiveId);

        return getTriangleDynamic(inst, primitiveId, vertIndices);
    }
    else
    {
        ShGeometryInstance inst = getGeomInstanceStatic(geometryIndex);
        uvec3 vertIndices = getVertIndicesStatic(inst, primitiveId);

        return getTriangleStatic(inst, primitiveId, vertIndices);
    }
}
#endif // DESC_SET_VERTEX_DATA
#endif // DESC_SET_GLOBAL_UNIFORM