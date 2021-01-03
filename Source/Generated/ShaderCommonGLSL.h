#define MAX_STATIC_VERTEX_COUNT (2097152)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define MAX_VERTEX_COLLECTOR_INDEX_COUNT (4194304)
#define MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT (262144)
#define MAX_TOP_LEVEL_INSTANCE_COUNT (4096)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_INDEX_BUFFER_STATIC (2)
#define BINDING_INDEX_BUFFER_DYNAMIC (3)
#define BINDING_GEOMETRY_INSTANCES_STATIC (4)
#define BINDING_GEOMETRY_INSTANCES_DYNAMIC (5)
#define BINDING_GLOBAL_UNIFORM (0)
#define BINDING_ACCELERATION_STRUCTURE (0)
#define BINDING_STORAGE_IMAGE (0)
#define INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC (1 << 0)

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
    mat3 positions;
    mat3 normals;
    mat3x2 textureCoords;
    vec3 tangent;
    uvec3 materialId;
};

struct ShGlobalUniform
{
    mat4 view;
    mat4 invView;
    mat4 viewPrev;
    mat4 projection;
    mat4 invProjection;
    mat4 projectionPrev;
    uint positionsStride;
    uint normalsStride;
    uint texCoordsStride;
    uint colorsStride;
};

struct ShGeometryInstance
{
    uint baseVertexIndex;
    uint baseIndexIndex;
    uint materialId0;
    uint materialId1;
    uint materialId2;
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

