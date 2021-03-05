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

#ifdef DESC_SET_GLOBAL_UNIFORM
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
    binding = BINDING_GEOMETRY_INSTANCES)
    readonly 
    buffer GeometryInstances_BT
{
    ShGeometryInstance geometryInstances[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_PREV_POSITIONS_BUFFER_DYNAMIC)
    #ifndef VERTEX_BUFFER_WRITEABLE
    readonly 
    #endif
    buffer PrevPositionsBufferStatic_BT
{
    float prevDynamicPositions[];
};

layout(
    set = DESC_SET_VERTEX_DATA,
    binding = BINDING_PREV_INDEX_BUFFER_DYNAMIC)
    #ifndef VERTEX_BUFFER_WRITEABLE
    readonly 
    #endif
    buffer PrevIndexBufferDynamic_BT
{
    uint prevDynamicIndices[];
};

#define TANGENTS_STRIDE 3

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

vec3 getStaticVerticesTangents(uint index)
{
    return vec3(
        staticVertices.tangents[index * TANGENTS_STRIDE + 0],
        staticVertices.tangents[index * TANGENTS_STRIDE + 1],
        staticVertices.tangents[index * TANGENTS_STRIDE + 2]);
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

vec3 getDynamicVerticesTangents(uint index)
{
    return vec3(
        dynamicVertices.tangents[index * TANGENTS_STRIDE + 0],
        dynamicVertices.tangents[index * TANGENTS_STRIDE + 1],
        dynamicVertices.tangents[index * TANGENTS_STRIDE + 2]);
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

void setStaticVerticesTangents(uint index, vec3 value)
{
    staticVertices.tangents[index * TANGENTS_STRIDE + 0] = value[0];
    staticVertices.tangents[index * TANGENTS_STRIDE + 1] = value[1];
    staticVertices.tangents[index * TANGENTS_STRIDE + 2] = value[2];
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

void setDynamicVerticesTangents(uint index, vec3 value)
{
    dynamicVertices.tangents[index * TANGENTS_STRIDE + 0] = value[0];
    dynamicVertices.tangents[index * TANGENTS_STRIDE + 1] = value[1];
    dynamicVertices.tangents[index * TANGENTS_STRIDE + 2] = value[2];
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

// Get geometry index in "geometryInstances" array by instanceID, localGeometryIndex.
// instanceCustomIndex is used for determining if should use offsets for main or skybox.
int getGeometryIndex(int instanceID, int instanceCustomIndex, int localGeometryIndex)
{
    // offset if skybox
    if ((instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_SKYBOX) != 0)
    {
        instanceID += MAX_TOP_LEVEL_INSTANCE_COUNT;
    }
    
    return globalUniform.instanceGeomInfoOffset[instanceID / 4][instanceID % 4] + localGeometryIndex;
}

// localGeometryIndex is index of geometry in pGeometries in BLAS
// primitiveId is index of a triangle
ShTriangle getTriangle(int instanceID, int instanceCustomIndex, int localGeometryIndex, int primitiveId)
{
    ShTriangle tr;

    // get info about geometry by the index in pGeometries in BLAS with index "instanceID"
    const int globalGeometryIndex = getGeometryIndex(instanceID, instanceCustomIndex, localGeometryIndex);

    const ShGeometryInstance inst = geometryInstances[globalGeometryIndex];

    const bool isDynamic = (instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;

    if (isDynamic)
    {
        uvec3 vertIndices = getVertIndicesDynamic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        tr = getTriangleDynamic(vertIndices);

        // only one material for dynamic geometry
        tr.materials[0] = uvec3(inst.materials[0]);
        tr.materials[1] = uvec3(MATERIAL_NO_TEXTURE);
        tr.materials[2] = uvec3(MATERIAL_NO_TEXTURE);
        
        tr.materialColors[0] = inst.materialColors[0];
    }
    else
    {
        uvec3 vertIndices = getVertIndicesStatic(inst.baseVertexIndex, inst.baseIndexIndex, primitiveId);

        tr = getTriangleStatic(vertIndices);

        tr.materials[0] = uvec3(inst.materials[0]);
        tr.materials[1] = uvec3(inst.materials[1]);
        tr.materials[2] = uvec3(inst.materials[2]);

        tr.materialColors[0] = inst.materialColors[0];
        tr.materialColors[1] = inst.materialColors[1];
        tr.materialColors[2] = inst.materialColors[2];
    }
    
    const mat3 model3 = mat3(inst.model);

    // to world space
    tr.normals[0] = model3 * tr.normals[0];
    tr.normals[1] = model3 * tr.normals[1];
    tr.normals[2] = model3 * tr.normals[2];

    tr.positions[0] = (inst.model * vec4(tr.positions[0], 1.0)).xyz;
    tr.positions[1] = (inst.model * vec4(tr.positions[1], 1.0)).xyz;
    tr.positions[2] = (inst.model * vec4(tr.positions[2], 1.0)).xyz;

    tr.materialsBlendFlags = inst.flags;

    tr.geomRoughness = inst.defaultRoughness;
    tr.geomMetallicity = inst.defaultMetallicity;

    // use the first layer's color
    tr.geomEmission = tr.materialColors[0].rgb * inst.defaultEmission;

    return tr;
}

mat4 getModelMatrix(int instanceID, int instanceCustomIndex, int localGeometryIndex)
{
    int globalGeometryIndex = getGeometryIndex(instanceID, instanceCustomIndex, localGeometryIndex);
    return geometryInstances[globalGeometryIndex].model;
}
#endif // DESC_SET_VERTEX_DATA
#endif // DESC_SET_GLOBAL_UNIFORM