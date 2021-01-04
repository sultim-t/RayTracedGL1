#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(RG_DEFINE_NON_DISPATCHABLE_HANDLE)
    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
        #define RG_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
    #else
        #define RG_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
    #endif
#endif

typedef uint32_t RgBool32;
typedef uint32_t RgFlags;
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgInstance)
typedef uint32_t RgGeometry;
// RgTexture, RgAnimatedTexture, RgDynamicTexture use the same pool of their indices,
// so the type can be determined from the union of these types
typedef uint32_t RgStaticTexture;
typedef uint32_t RgAnimatedTexture;
typedef uint32_t RgDynamicTexture;
#define RG_NO_TEXTURE   0
#define RG_FALSE        0
#define RG_TRUE         1

typedef enum RgResult
{
    RG_SUCCESS,
    RG_ERROR,
    RG_WRONG_ARGUMENT,
    RG_TOO_MANY_INSTANCES,
    RG_WRONG_INSTANCE,
    RG_FRAME_WASNT_STARTED,
    RG_FRAME_WASNT_ENDED,
    RG_UPDATING_NOT_MOVABLE,
} RgResult;

//typedef enum RgStructureType
//{
//    RG_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
//} RgStructureType;

typedef void (*PFN_rgCreateVkSurfaceKHR)(uint64_t vkInstance, uint64_t *pResultVkSurfaceKHR);
typedef void (*PFN_rgPrint)(const char *msg);

typedef struct RgInstanceCreateInfo
{
    const char                  *name;
    uint32_t                    physicalDeviceIndex;
    // Vulkan OS-specific window extensions
    const char                  **ppWindowExtensions;
    uint32_t                    windowExtensionCount;
    // Pointer to the function for creating VkSurfaceKHR
    PFN_rgCreateVkSurfaceKHR    pfnCreateSurface;
    RgBool32                    enableValidationLayer;
    PFN_rgPrint                 pfnDebugPrint;
    // Size that must be allocated for rasterized geometry, in bytes.
    // It can't be changed after rgCreateInstance.
    // If buffer is full, rasterized data will be ignored
    uint32_t                    rasterizedDataBufferSize;
    // Postfixes will be used to determine textures that should be 
    // loaded from files if the texture should be overridden
    // i.e. if postfix="_n" then "Floor_01" => "Floor_01_n.*", 
    // where "*" is some image extension
    char                        *overrideAlbedoRoughnessTexturePostfix;
    char                        *overrideNormalMetallicTexturePostfix;
    char                        *overrideEmissionSpecularityTexturePostfix;

    // Vertex data strides in bytes. Must be 4-byte aligned.
    uint32_t                    vertexPositionStride;
    uint32_t                    vertexNormalStride;
    uint32_t                    vertexTexCoordStride;
    uint32_t                    vertexColorStride;

    // Each attribute has its own stride for ability to describe vertices 
    // that are represented as separated arrays of attribute values (i.e. Positions[], Normals[], ...)
    // or packed into array of structs (i.e. Vertex[] where Vertex={Position, Normal, ...}).
    // Note: array of structs will cause a lot of unused memory as RTGL1 uses separated arrays
    RgBool32                    vertexArrayOfStructs;

} RgInstanceCreateInfo;

RgResult rgCreateInstance(
    const RgInstanceCreateInfo  *info,
    RgInstance                  *result);

RgResult rgDestroyInstance(
    RgInstance                  rgInstance);

void rgUpdateWindowSize(
    RgInstance                  rgInstance,
    uint32_t                    width,
    uint32_t                    height);



typedef union RgTexture
{
    RgStaticTexture     staticTexture;
    RgAnimatedTexture   animatedTexture;
    RgDynamicTexture    dynamicTexture;
} RgTexture;

typedef struct RgLayeredMaterial
{
    // Geometry or each triangle can have up to 3 textures, RG_NO_TEXTURE is no material
    RgTexture   layerTextures[3];
} RgLayeredMaterial;

typedef enum RgGeometryType
{
    RG_GEOMETRY_TYPE_STATIC,
    RG_GEOMETRY_TYPE_STATIC_MOVABLE,
    RG_GEOMETRY_TYPE_DYNAMIC
} RgGeometryType;

typedef struct RgTransform
{
    float       matrix[3][4];
} RgTransform;

typedef struct RgGeometryUploadInfo
{
    RgGeometryType          geomType;

    uint32_t                vertexCount;
    // Strides are set in RgInstanceUploadInfo
    // 3 first floats will be used
    float                   *vertexData;
    // 3 floats; can be null
    float                   *normalData;
    // 2 floats; can be null
    float                   *texCoordData;
    // RGBA packed into 32-bit uint; can be null
    uint32_t                *colorData;

    // Can be null, if indices are not used
    uint32_t                indexCount;
    uint32_t                *indexData;

    RgLayeredMaterial       geomMaterial;

    // TODO: seems to be redundant? as in fixed function pipeline
    // Only up to 4 textures are used per geometry
    
    // If not null, then each triangle will be using its specified material.
    // Otherwise, geomMaterial will be applied for whole geometry
    RgLayeredMaterial       *triangleMaterials;

    RgTransform             transform;
} RgGeometryUploadInfo;

typedef struct RgUpdateTransformInfo
{
    RgGeometry      movableStaticGeom;
    RgTransform     transform;
} RgUpdateTransformInfo;

typedef enum RgBlendFactor
{
    RG_BLEND_FACTOR_ONE,
    RG_BLEND_FACTOR_ZERO,
    RG_BLEND_FACTOR_SRC_COLOR,
    RG_BLEND_FACTOR_INV_SRC_COLOR,
    RG_BLEND_FACTOR_DST_COLOR,
    RG_BLEND_FACTOR_INV_DST_COLOR,
    RG_BLEND_FACTOR_SRC_ALPHA,
    RG_BLEND_FACTOR_INV_SRC_ALPHA,
} RgBlendFactor;

typedef enum RgSamplerAddressMode
{
    RG_SAMPLER_ADDRESS_MODE_REPEAT,
    RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    RG_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
} RgSamplerAddressMode;

typedef struct RgRasterizedGeometryUploadInfo
{
    RgBool32            depthTest;
    RgBool32            depthWrite;
    RgBool32            alphaTest;
    RgBool32            blendEnable;
    RgBlendFactor       blendFuncSrc;
    RgBlendFactor       blendFuncDst;
    uint32_t            textureCount;
    RgTexture           textures[4];

    uint32_t            vertexCount;
    // 3 floats
    float               *vertexData;
    uint32_t            vertexStride;
    // 3 floats
    float               *normalData;
    uint32_t            normalStride;
    // 2 floats
    float               *texCoordData;
    uint32_t            texCoordStride;
    // RGBA packed into 32-bit uint
    uint32_t            *colorData;
    uint32_t            colorStride;

    uint32_t            indexCount;
    uint32_t            *indexData;
} RgRasterizedGeometryUploadInfo;

// Uploaded static geometries are only visible after submitting them using rgSubmitStaticGeometries.
// Uploaded dynamic geometries are only visible in the current frame.
// "result" may be null, if its transform won't be changed
RgResult rgUploadGeometry(
    RgInstance                              rgInstance,
    const RgGeometryUploadInfo              *uploadInfo,
    RgGeometry                              *result);

// Updating transform is available only for movable static geometry.
// Other geometry types don't need it because they are either fully static
// or uploaded every frame, so transforms are always as they are intended.
RgResult rgUpdateGeometryTransform(
    RgInstance                              rgInstance,
    const RgUpdateTransformInfo             *updateInfo);

// Upload geometry that will be drawn using rasterization,
// whole buffer for such geometry be discarded after frame finish
RgResult rgUploadRasterizedGeometry(
    RgInstance                              rgInstance,
    const RgRasterizedGeometryUploadInfo    *uploadInfo);



typedef enum RgLightType
{
    RG_LIGHT_TYPE_STATIC,
    RG_LIGHT_TYPE_DYNAMIC
} RgLightType;

typedef struct RgDirectionalLightUploadInfo
{
    RgLightType     type;
    uint32_t        color;
    float           intensity;
    float           direction[3];
    float           angularSize;
} RgDirectionalLightUploadInfo;

typedef struct RgSphereLightUploadInfo
{
    RgLightType     type;
    uint32_t        color;
    float           intensity;
    float           position[3];
    float           radius;
} RgSphereLightUploadInfo;

RgResult rgUploadLight(
    RgInstance                              rgInstance,
    RgDirectionalLightUploadInfo            *lightInfo);

/*RgResult rgUploadLight(
    RgInstance                              rgInstance,
    RgSphereLightUploadInfo                  *lightInfo);*/



// After uploading all static geometry and static lights, scene must be submitted before rendering.
// However, movable static geometry can be moved using rgUpdateGeometryTransform.
// When the static scene data should be changed, it must be cleared using rgClearScene
// and new static geometries must be uploaded.
RgResult rgSubmitStaticGeometries(
    RgInstance                              rgInstance);

// Clear current scene from all static geometries and static lights
// and make it available for recording new geometries.
// New scene can be shown only after its submission using rgSubmitStaticGeometries.
RgResult rgStartNewScene(
    RgInstance                              rgInstance);



typedef struct RgExtent2D
{
    uint32_t    width;
    uint32_t    height;
} RgExtent2D;

typedef struct RgExtent3D
{
    uint32_t    width;
    uint32_t    height;
    uint32_t    depth;
} RgExtent3D;

typedef struct RgStaticTextureCreateInfo
{
    // Only R8G8B8A8 textures
    uint32_t                dataSize;
    uint32_t                *data;
    uint32_t                mipmapCount;
    uint8_t                 mipmapData[32];
    RgExtent2D              mipmapSizes[32];

    // Sampler info is here too for simplicity
    RgSamplerAddressMode    addressModeU;
    RgSamplerAddressMode    addressModeV;
    RgBool32                enableOverride;
    float                   defaultRoughness;
    float                   defaultMetallicity;
    char                    *name;
    char                    *path;
} RgStaticTextureCreateInfo;
    
typedef struct RgDynamicTextureInfo
{
    RgExtent2D              size;
    uint32_t                *data;
    RgSamplerAddressMode    addressModeU;
    RgSamplerAddressMode    addressModeV;
    float                   defaultRoughness;
    float                   defaultMetallicity;
    float                   defaultSpecularity;
} RgDynamicTextureInfo;
    
typedef struct RgAnimatedTextureCreateInfo
{
    uint32_t                            frameCount;
    RgStaticTextureCreateInfo           *frames;
} RgAnimatedTextureCreateInfo;

RgResult rgCreateStaticTexture(
    RgInstance                          rgInstance,
    const RgStaticTextureCreateInfo     *createInfo,
    RgStaticTexture                     *result);

RgResult rgCreateAnimatedTexture(
    RgInstance                          rgInstance,
    const RgAnimatedTextureCreateInfo   *createInfo,
    RgAnimatedTexture                   *result);

RgResult rgChangeAnimatedTextureFrame(
    RgAnimatedTexture                   animatedTexture,
    uint32_t                            frameIndex);
    
RgResult rgCreateDynamicTexture(
    RgInstance                          rgInstance,
    const RgDynamicTextureInfo          *dynamicTextureInfo,
    RgDynamicTexture                    *result);

RgResult rgUpdateDynamicTexture(
    RgInstance                          rgInstance,
    RgDynamicTexture                    dynamicTexture,
    const RgDynamicTextureInfo          *updateInfo);



RgResult rgStartFrame(
    RgInstance                          rgInstance,
    uint32_t                            surfaceWidth, 
    uint32_t                            surfaceHeight);

typedef struct RgDrawFrameInfo
{
    uint32_t        renderWidth;
    uint32_t        renderHeight;
    float           view[16];
    float           viewInversed[16];
    float           projection[16];
    float           projectionInversed[16];
} RgDrawFrameInfo;

RgResult rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *frameInfo);

#ifdef __cplusplus
}
#endif