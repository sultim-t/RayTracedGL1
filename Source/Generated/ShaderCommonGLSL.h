#define MAX_STATIC_VERTEX_COUNT (4194304)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define MAX_VERTEX_COLLECTOR_INDEX_COUNT (4194304)
#define MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT (262144)
#define MAX_VERTEX_COLLECTOR_GEOM_INFOS_COUNT (262144)
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
#define BINDING_TEXTURES (0)
#define INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC (1 << 0)

struct ShVertexBufferStatic
{
    float positions[12582912];
    float normals[12582912];
    float texCoords[8388608];
    uint colors[4194304];
    uint materialIds[1398104];
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
    uvec3 materialIds;
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
    mat4 model;
    uvec4 materials[3];
    uint baseVertexIndex;
    uint baseIndexIndex;
    uint primitiveCount;
    uint __pad0;
};

