#include "ShaderCommonGLSL.h"

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

// get info about geometry from pGeometries in one of BLAS
ShGeometryInstance getGeomInstanceStatic(uint geometryIndex)
{
    GeometryInstance inst = {};
    inst.baseVertexIndex = geometryInstancesStatic[geometryIndex].baseVertexIndex;
    inst.baseIndexIndex = geometryInstancesStatic[geometryIndex].baseIndexIndex;
    inst.materialId = geometryInstancesStatic[geometryIndex].materialId;

    return inst;
}
ShGeometryInstance getGeomInstanceDynamic(uint geometryIndex)
{
    GeometryInstance inst = {};
    inst.baseVertexIndex = geometryInstancesDynamic[geometryIndex].baseVertexIndex;
    inst.baseIndexIndex = geometryInstancesDynamic[geometryIndex].baseIndexIndex;
    inst.materialId = geometryInstancesDynamic[geometryIndex].materialId;

    return inst;
}

ivec3 getVertIndicesDynamic(ShGeometryInstance inst, uint primitiveId)
{
    // if to use indices
    if (inst.baseIndexIndex != UINT32_MAX)
    {
        return ivec3(
            inst.baseVertexIndex + indicesDynamic[inst.baseIndexIndex + primitiveId * 3 + 0],
            inst.baseVertexIndex + indicesDynamic[inst.baseIndexIndex + primitiveId * 3 + 1],
            inst.baseVertexIndex + indicesDynamic[inst.baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return ivec3(
            inst.baseVertexIndex + primitiveId * 3 + 0,
            inst.baseVertexIndex + primitiveId * 3 + 1,
            inst.baseVertexIndex + primitiveId * 3 + 2);
    }
}

ivec3 getVertIndicesStatic(ShGeometryInstance inst, uint primitiveId)
{
    // if to use indices
    if (inst.baseIndexIndex != UINT32_MAX)
    {
        return ivec3(
            inst.baseVertexIndex + indicesStatic[inst.baseIndexIndex + primitiveId * 3 + 0],
            inst.baseVertexIndex + indicesStatic[inst.baseIndexIndex + primitiveId * 3 + 1],
            inst.baseVertexIndex + indicesStatic[inst.baseIndexIndex + primitiveId * 3 + 2]);
    }
    else
    {
        return ivec3(
            inst.baseVertexIndex + primitiveId * 3 + 0,
            inst.baseVertexIndex + primitiveId * 3 + 1,
            inst.baseVertexIndex + primitiveId * 3 + 2);
    }
}

ShTriangle getTriangleStatic(uint primitiveId, ivec3 vertIndices)
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

    tr.tangent = vec3();

    tr.materialIds = ivec3(inst.materialId0, inst.materialId1, inst.materialId2);

    return tr;
}

ShTriangle getTriangleDynamic(uint primitiveId, ivec3 vertIndices)
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

    tr.tangent = vec3();

    tr.materialIds = ivec3(inst.materialId0, inst.materialId1, inst.materialId2);

    return tr;
}

// instanceId is index of instance in TLAS
// geometryIndex is index of geometry in pGeometries in BLAS
// primitiveId is index of a triangle
ShTriangle getTriangle(uint instanceId, uint instanceCustomIndex, uint geometryIndex, uint primitiveId)
{
    // TODO: get isDynamic by custom index
    bool isDynamic = instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    // or by uniform variable
    // bool isDynamic = instanceId == globalUniform.dynamicInstanceId


    if (isDynamic)
    {
        ShGeometryInstance inst = getGeomInstanceDynamic(geometryIndex);
        ivec3 vertIndices = getVertIndicesDynamic(inst, primitiveId);

        return getTriangleDynamic(primitiveId, vertIndices);
    }
    else
    {
        ShGeometryInstance inst = getGeomInstanceStatic(geometryIndex);
        ivec3 vertIndices = getVertIndicesStatic(inst, primitiveId);

        return getTriangleStatic(primitiveId, vertIndices);
    }
}
#endif // DESC_SET_VERTEX_DATA
#endif // DESC_SET_GLOBAL_UNIFORM