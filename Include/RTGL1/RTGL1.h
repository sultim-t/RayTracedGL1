// Copyright (c) 2020-2021 Sultim Tsyrendashiev
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
typedef uint32_t RgMaterial;

#define RG_NULL_HANDLE      0
#define RG_NO_MATERIAL      0
#define RG_FALSE            0
#define RG_TRUE             1

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
    RG_CANT_UPDATE_DYNAMIC_MATERIAL,
    RG_CANT_UPDATE_ANIMATED_MATERIAL,
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

    // Memory that must be allocated for vertex and index buffers of rasterized geometry.
    // It can't be changed after rgCreateInstance.
    // If buffer is full, rasterized data will be ignored
    uint32_t                    rasterizedMaxVertexCount;
    uint32_t                    rasterizedMaxIndexCount;

    // The folder to find overriding textures in.
    const char                  *overridenTexturesFolderPath;
    // Postfixes will be used to determine textures that should be 
    // loaded from files if the texture should be overridden
    // i.e. if postfix="_n" then "Floor_01" => "Floor_01_n.*", 
    // where "*" is some image extension
    // If null, then empty string will be used.
    const char                  *overrideAlbedoAlphaTexturePostfix;
    // If null, then "_n" will be used.
    const char                  *overrideNormalMetallicTexturePostfix;
    // If null, then "_e" will be used.
    const char                  *overrideEmissionRoughnessTexturePostfix;
    // If a texture doesn't have overriden data, these default values are used.
    float                       defaultRoughness;
    float                       defaultMetallicity;

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
    const RgInstanceCreateInfo          *info,
    RgInstance                          *result);

RgResult rgDestroyInstance(
    RgInstance                          rgInstance);



typedef struct RgLayeredMaterial
{
    // Geometry or each triangle can have up to 3 materials, RG_NO_MATERIAL is no material
    RgMaterial  layerMaterials[3];
} RgLayeredMaterial;

typedef enum RgGeometryType
{
    RG_GEOMETRY_TYPE_STATIC,
    RG_GEOMETRY_TYPE_STATIC_MOVABLE,
    RG_GEOMETRY_TYPE_DYNAMIC
} RgGeometryType;

typedef enum RgGeometryPassThroughType
{
    RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE,
    RG_GEOMETRY_PASS_THROUGH_TYPE_ALPHA_TESTED,
    RG_GEOMETRY_PASS_THROUGH_TYPE_BLEND_ADDITIVE,
    RG_GEOMETRY_PASS_THROUGH_TYPE_BLEND_UNDER
} RgGeometryPassThroughType;

typedef enum RgGeometryVisibilityType
{
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER,
    RG_GEOMETRY_VISIBILITY_TYPE_SKYBOX,
} RgGeometryPrimaryVisibilityType;

typedef enum RgGeometryMaterialBlendType
{
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_OPAQUE,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_ALPHA,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_ADD,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_SHADE
} RgGeometryMaterialBlendType;

typedef struct RgTransform
{
    float       matrix[3][4];
} RgTransform;

typedef struct RgFloat3
{
    float       data[3];
} RgFloat3;

typedef struct RgFloat4
{
    float       data[4];
} RgFloat4;

typedef struct RgGeometryUploadInfo
{
    RgGeometryType                  geomType;
    RgGeometryPassThroughType       passThroughType;
    RgGeometryVisibilityType        visibilityType;

    uint32_t                        vertexCount;
    // Strides are set in RgInstanceUploadInfo.
    // 3 first floats will be used
    void                            *vertexData;
    // 3 first floats will be used
    void                            *normalData;
    // Up to 3 texture coordinated per vertex for static geometry.
    // Dynamic geometry uses only 1 layer.
    // 2 first floats will be used
    void                            *texCoordLayerData[3];

    // Can be null, if indices are not used.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t                        indexCount;
    void                            *indexData;

    // RGBA color for each material layer.
    RgFloat4                        layerColors[3];
    RgGeometryMaterialBlendType     layerBlendingTypes[3];
    // These default values will be used if no overriding 
    // texture is found.
    float                           defaultRoughness;
    float                           defaultMetallicity;
    // Emission = defaultEmission * color
    float                           defaultEmission;

    RgLayeredMaterial               geomMaterial;
    RgTransform                     transform;
} RgGeometryUploadInfo;

typedef struct RgUpdateTransformInfo
{
    RgGeometry      movableStaticGeom;
    RgTransform     transform;
} RgUpdateTransformInfo;

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

// Struct is used to transform from NDC to window coordinates.
// All members are in pixels.
typedef struct RgViewport
{
    float       x;
    float       y;
    float       width;
    float       height;
} RgViewport;

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

typedef struct RgRasterizedGeometryUploadInfo
{
    // Doesn't need it.
    //RgBool32            depthTest;
    //RgBool32            depthWrite;

    // Hardcoded for the first versions
    /*RgBool32            alphaTest;
    RgBool32            blendEnable;
    RgBlendFactor       blendFuncSrc;
    RgBlendFactor       blendFuncDst;*/

    // Only the albedo-alpha textures from the materials
    // are used for rasterized geometry.
    RgLayeredMaterial   textures;

    uint32_t            vertexCount;
    // Position data, 3 floats
    void                *vertexData;
    uint32_t            vertexStride;
    // 3 floats
    //void                *normalData;
    //uint32_t            normalStride;
    // 2 floats, can be null
    void                *texCoordData;
    uint32_t            texCoordStride;
    // RGBA packed into 32-bit uint, can be null
    void                *colorData;
    uint32_t            colorStride;

    // Can be 0 and null.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t            indexCount;
    void                *indexData;

    // Viewport to draw in. If every member is about 0,
    // then full screen is used. 
    RgViewport          viewport;

    // View-projection matrix to apply to this rasterized geometry.
    // Matrix is column major.
    float               viewProjection[16];
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
    RgFloat3        color;
    RgFloat3        direction;
    float           angularDiameterDegrees;
} RgDirectionalLightUploadInfo;

typedef struct RgSphericalLightUploadInfo
{
    RgLightType     type;
    RgFloat3        color;
    RgFloat3        position;
    float           radius;
} RgSphericalLightUploadInfo;

RgResult rgUploadDirectionalLight(
    RgInstance                          rgInstance,
    RgDirectionalLightUploadInfo        *lightInfo);

RgResult rgUploadSphericalLight(
    RgInstance                          rgInstance,
    RgSphericalLightUploadInfo          *lightInfo);



// After uploading all static geometry and static lights, scene must be submitted before rendering.
// However, movable static geometry can be moved using rgUpdateGeometryTransform.
// When the static scene data should be changed, it must be cleared using rgClearScene
// and new static geometries must be uploaded.
RgResult rgSubmitStaticGeometries(
    RgInstance                          rgInstance);

// Clear current scene from all static geometries and static lights
// and make it available for recording new geometries.
// New scene can be shown only after its submission using rgSubmitStaticGeometries.
RgResult rgStartNewScene(
    RgInstance                          rgInstance);


typedef enum RgSamplerAddressMode
{
    RG_SAMPLER_ADDRESS_MODE_REPEAT,
    RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    RG_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
} RgSamplerAddressMode;

typedef enum RgSamplerFilter
{
    RG_SAMPLER_FILTER_LINEAR,
    RG_SAMPLER_FILTER_NEAREST,
} RgSamplerFilter;

typedef struct RgStaticMaterialCreateInfo
{
    // If data is used then size must specify width and height.
    // "data" must be (width * height * 4) bytes.
    RgExtent2D              size;
    // Only R8G8B8A8 textures.
    // Firstly, the library will try to find image file using "relativePath",
    // and if nothing is found "data" is used. Additional overriding data
    // such as normal, metallic, roughness, emission maps will be loaded
    // using "relativePath" and overriding postfixes.
    void                    *data;
    RgBool32                isSRGB;
    // The library will try to find image files using path:
    //      "<overridenTexturesFolderPath>/<relativePath>".
    // "relativePath" must be in the following format:
    //      "<folders>/<name>.<extension>".
    // "name" will be used as a base for overriding texture names.
    // "folders" and "extension" are not used.
    // Image files must be in PNG or TGA formats.
    const char              *relativePath;
    // If true, mipmaps will be generated by the library.
    RgBool32                useMipmaps;
    RgSamplerFilter         filter;
    RgSamplerAddressMode    addressModeU;
    RgSamplerAddressMode    addressModeV;
    // If true, the library won't try to find files with additional info,
    // so the default values specified in RgInstanceCreateInfo will be used.
    RgBool32                disableOverride;
} RgStaticMaterialCreateInfo;
    
typedef struct RgDynamicMaterialCreateInfo
{
    // The width and height must be > 0.
    RgExtent2D              size;
    // Only R8G8B8A8 textures.
    // If data is not null, the newly created dynamic texture will be
    // updated using this data. Otherwise, it'll be empty until
    // "rgUpdateDynamicMaterial" call.
    void                    *data;
    RgBool32                isSRGB;
    // If true, mipmaps will be generated by the library.
    // Should be false for better performance.
    RgBool32                useMipmaps;
    RgSamplerFilter         filter;
    RgSamplerAddressMode    addressModeU;
    RgSamplerAddressMode    addressModeV;
} RgDynamicMaterialCreateInfo;

typedef struct RgDynamicMaterialUpdateInfo
{
    RgMaterial              dynamicMaterial;
    uint32_t                *data;
} RgDynamicMaterialUpdateInfo;

typedef struct RgAnimatedMaterialCreateInfo
{
    uint32_t                            frameCount;
    RgStaticMaterialCreateInfo          *frames;
} RgAnimatedMaterialCreateInfo;

RgResult rgCreateStaticMaterial(
    RgInstance                          rgInstance,
    const RgStaticMaterialCreateInfo    *createInfo,
    RgMaterial                          *result);

RgResult rgCreateAnimatedMaterial(
    RgInstance                          rgInstance,
    const RgAnimatedMaterialCreateInfo  *createInfo,
    RgMaterial                          *result);

RgResult rgChangeAnimatedMaterialFrame(
    RgInstance                          rgInstance,
    RgMaterial                          animatedMaterial,
    uint32_t                            frameIndex);
    
RgResult rgCreateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialCreateInfo   *createInfo,
    RgMaterial                          *result);

RgResult rgUpdateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialUpdateInfo   *updateInfo);

// Destroying RG_NO_MATERIAL has no effect.
RgResult rgDestroyMaterial(
    RgInstance                          rgInstance,
    RgMaterial                          material);



RgResult rgStartFrame(
    RgInstance                          rgInstance,
    uint32_t                            surfaceWidth, 
    uint32_t                            surfaceHeight, 
    RgBool32                            vsync);

typedef enum RgDrawFrameFlagBits
{
    RG_DRAW_FRAME_DISABLE_ALBEDO_MAPS       = 1 << 0,
    RG_DRAW_FRAME_DISABLE_NORMAL_MAPS       = 1 << 1,
    RG_DRAW_FRAME_DISABLE_RASTERIZATION     = 1 << 2,
    RG_DRAW_FRAME_FORCE_ROUGHNESS_ONE       = 1 << 3,
    RG_DRAW_FRAME_FORCE_ROUGHNESS_ZERO      = 1 << 4,
    RG_DRAW_FRAME_FORCE_METALLICITY_ONE     = 1 << 5,
    RG_DRAW_FRAME_FORCE_METALLICITY_ZERO    = 1 << 6,

    RG_DRAW_FRAME_FORCE_ROUGHNESS_MASK = RG_DRAW_FRAME_FORCE_ROUGHNESS_ONE | RG_DRAW_FRAME_FORCE_ROUGHNESS_ZERO,
    RG_DRAW_FRAME_FORCE_METALLICITY_MASK = RG_DRAW_FRAME_FORCE_METALLICITY_ONE | RG_DRAW_FRAME_FORCE_METALLICITY_ZERO,
} RgDrawFrameFlagBits;
typedef RgFlags RgDrawFrameFlags;

typedef enum RgSkyType
{
    RG_SKY_TYPE_COLOR,
    RG_SKY_TYPE_CUBEMAP,
    RG_SKY_TYPE_GEOMETRY
} RgSkyType;

typedef struct RgDrawFrameInfo
{
    // View and projection matrices are column major.
    float               view[16];
    float               projection[16];
    RgDrawFrameFlags    flags;
    uint32_t            renderWidth;
    uint32_t            renderHeight;
    double              currentTime;
    RgBool32            disableEyeAdaptation;
    // True, if default values for minLogLuminance, maxLogLuminance
    // and luminanceWhitePoint should be used.
    RgBool32            overrideTonemappingParams;
    float               minLogLuminance;
    float               maxLogLuminance;
    float               luminanceWhitePoint;
    RgFloat3            skyColor;
    float               skyColorMultiplier;
    RgSkyType           skyType;
    
} RgDrawFrameInfo;

RgResult rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *frameInfo);

#ifdef __cplusplus
}
#endif