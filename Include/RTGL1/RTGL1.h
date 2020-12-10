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
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgGeometry)
// RgTexture, RgAnimatedTexture, RgDynamicTexture use the same pool of their indices,
// so the type can be determined from the union of these types
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgStaticTexture)
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgAnimatedTexture)
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgDynamicTexture)
#define RG_NO_TEXTURE   0
#define RG_FALSE        0
#define RG_TRUE         1

typedef enum RgResult
{
    RG_SUCCESS,
    RG_ERROR,
    RG_TOO_MANY_ISTANCES,
    RG_NULL_INSTANCE
} RgResult;

//typedef enum RgStructureType
//{
//    RG_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
//} RgStructureType;

typedef void (*PFN_rgCreateVkSurfaceKHR)(const void *pVkInstance, void *pResultVkSurfaceKHR);
typedef void (*PFN_rgPrint)(const char *msg);

typedef struct RgInstanceCreateInfo
{
    const char                  *name;
    const uint32_t              physicalDeviceIndex;
    // Vulkan OS-specific window extensions
    const char                  **ppWindowExtensions;
    uint32_t                    windowExtensionCount;
    // pointer to the function for creating VkSurfaceKHR
    PFN_rgCreateVkSurfaceKHR    pfnCreateSurface;
    RgBool32                    enableValidationLayer;
    PFN_rgPrint                 pfnDebugPrint;
    // size that must be allocated for rasterized geometry, in bytes;
    // it can't be changed after rgCreateInstance; if buffer is full,
    // rasterized data will be ignored
    uint32_t                    rasterizedDataBufferSize;
    // postfixes will be used to determine textures that should be 
    // loaded from files if the texture should be overridden
    // i.e. if postfix="_n" then "Floor_01" => "Floor_01_n.*", 
    // where "*" is some image extension
    char                        *overrideAlbedoRoughnessTexturePostfix;
    char                        *overrideNormalMetallicTexturePostfix;
    char                        *overrideEmissionSpecularityTexturePostfix;

    // each attribute has its own stride for ability to describe vertices 
    // that are represented as separated arrays of attribute values (i.e. Positions[], Normals[], ...)
    // or packed into array of structs (i.e. Vertex[] where Vertex={Position, Normal, ...}).
    // Note: array of structs will cause a lot of unused memory as RTGL1 uses separated arrays
    RgBool32                    vertexArrayOfStructs;
    uint32_t                    vertexPositionStride;
    uint32_t                    vertexNormalStride;
    uint32_t                    vertexTexCoordStride;
    uint32_t                    vertexColorStride;

} RgInstanceCreateInfo;

RgResult rgCreateInstance(
    const RgInstanceCreateInfo  *info,
    RgInstance                  *result);

RgResult rgDestroyInstance(
    RgInstance                  rgInstance);

typedef enum RgGeometryType
{
    RG_GEOMETRY_TYPE_STATIC,
    RG_GEOMETRY_TYPE_STATIC_MOVABLE,
    RG_GEOMETRY_TYPE_DYNAMIC
} RgGeometryType;

typedef union RgTexture
{
    RgStaticTexture     staticTexture;
    RgAnimatedTexture   animatedTexture;
    RgDynamicTexture    dynamicTexture;
} RgTexture;

typedef struct RgLayeredMaterial
{
    // geometry or each triangle can have up to 3 textures, RG_NO_TEXTURE is no material
    RgTexture   layerTextures[3];
} RgLayeredMaterial;

typedef struct RgTransform
{
    float       matrix[3][4];
} RgTransform;

typedef struct RgGeometryCreateInfo
{
    RgGeometryType          geomType;

    uint32_t                vertexCount;
    // 3 first floats first floats will be used
    float                   *vertexData;
    // 3 floats, can be null
    float                   *normalData;
    // 2 floats, can be null
    float                   *texCoordData;
    // RGBA packed into 32-bit uint, can be null
    uint32_t                *colorData;

    uint32_t                indexCount;
    uint32_t                *indexData;

    RgLayeredMaterial       geomMaterial;
    // if not null, then each triangle will be using its specified material,
    // otherwise, geomMaterial will be applied for whole geometry
    RgLayeredMaterial       *triangleMaterials;

    RgTransform             transform;
} RgGeometryCreateInfo;

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

RgResult rgCreateGeometry(
    RgInstance                              rgInstance,
    const RgGeometryCreateInfo              *createInfo,
    RgGeometry                              *result);

RgResult rgUpdateGeometryTransform(
    RgInstance                              rgInstance,
    const RgUpdateTransformInfo             *updateInfo);

// upload geometry that will be drawn using rasterization,
// whole buffer for such geometry be discarded after frame finish
RgResult rgUploadRasterizedGeometry(
    RgInstance                              rgInstance,
    const RgRasterizedGeometryUploadInfo    *uploadInfo);

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
    // only R8G8B8A8 textures
    uint32_t                dataSize;
    uint32_t                *data;
    uint32_t                mipmapCount;
    uint8_t                 mipmapData[32];
    RgExtent2D              mipmapSizes[32];

    // sampler info is here too for simplicity
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

typedef struct RgDrawFrameInfo
{
    uint32_t    width;
    uint32_t    height;
} RgDrawFrameInfo;

RgResult rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *frameInfo);

#ifdef __cplusplus
}
#endif