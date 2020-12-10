#define MAX_STATIC_VERTEX_COUNT (2097152)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_GLOBAL_UNIFORM (0)
#define BINDING_ACCELERATION_STRUCTURE (0)

#ifndef DESC_SET_VERTEX_DATA
    #error Define "DESC_SET_VERTEX_DATA" before including this header.
#endif
#ifndef DESC_SET_GLOBAL_UNIFORM
    #error Define "DESC_SET_GLOBAL_UNIFORM" before including this header.
#endif
#ifndef DESC_SET_ACCELERATION_STRUCTURE
    #error Define "DESC_SET_ACCELERATION_STRUCTURE" before including this header.
#endif

struct ShVertexBufferStatic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint colors[2097152];
    uint materialIds[699052];
};

struct ShVertexBufferDynamic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint colors[2097152];
    uint materialIds[699052];
};

struct ShTriangle
{
    mat33 positions;
    mat33 normals;
    mat32 textureCoords;
    uint materialId;
};

struct ShGlobalUniform
{
    mat44 viewProj;
    mat44 viewProjPrev;
    uint positionsStride;
    uint normalsStride;
    uint texCoordsStride;
    uint colorsStride;
};

layout(set = DESC_SET_VERTEX_DATA,
    binding = BINDING_VERTEX_BUFFER_STATIC)
    readonly buffer VertexBufferStatic_BT
{
    ShVertexBufferStatic staticVertices;
}

layout(set = DESC_SET_VERTEX_DATA,
    binding = BINDING_VERTEX_BUFFER_DYNAMIC)
    readonly buffer VertexBufferDynamic_BT
{
    ShVertexBufferDynamic dynamicVertices;
}

layout(set = DESC_SET_GLOBAL_UNIFORM,
    binding = BINDING_GLOBAL_UNIFORM)
    readonly uniform GlobalUniform_BT
{
    ShGlobalUniform globalUniform;
}

vec3 getStaticVerticesPositions(uint index)
{
    return vec3(staticVertices.positions[index * globalUniform.positionsStride + 0],
        staticVertices.positions[index * globalUniform.positionsStride + 1],
        staticVertices.positions[index * globalUniform.positionsStride + 2]);
}

vec3 getStaticVerticesNormals(uint index)
{
    return vec3(staticVertices.normals[index * globalUniform.normalsStride + 0],
        staticVertices.normals[index * globalUniform.normalsStride + 1],
        staticVertices.normals[index * globalUniform.normalsStride + 2]);
}

vec2 getStaticVerticesTexCoords(uint index)
{
    return vec2(staticVertices.texCoords[index * globalUniform.texCoordsStride + 0],
        staticVertices.texCoords[index * globalUniform.texCoordsStride + 1]);
}

vec3 getDynamicVerticesPositions(uint index)
{
    return vec3(dynamicVertices.positions[index * globalUniform.positionsStride + 0],
        dynamicVertices.positions[index * globalUniform.positionsStride + 1],
        dynamicVertices.positions[index * globalUniform.positionsStride + 2]);
}

vec3 getDynamicVerticesNormals(uint index)
{
    return vec3(dynamicVertices.normals[index * globalUniform.normalsStride + 0],
        dynamicVertices.normals[index * globalUniform.normalsStride + 1],
        dynamicVertices.normals[index * globalUniform.normalsStride + 2]);
}

vec2 getDynamicVerticesTexCoords(uint index)
{
    return vec2(dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 0],
        dynamicVertices.texCoords[index * globalUniform.texCoordsStride + 1]);
}

