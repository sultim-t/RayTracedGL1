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
typedef uint32_t RgMaterial;
typedef uint32_t RgCubemap;

#define RG_NULL_HANDLE      0
#define RG_NO_MATERIAL      0
#define RG_EMPTY_CUBEMAP    0
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
    RG_UPDATING_TRANSFORM_FOR_NON_MOVABLE,
    RG_UPDATING_TEXCOORDS_FOR_NON_STATIC,
    RG_CANT_UPDATE_DYNAMIC_MATERIAL,
    RG_CANT_UPDATE_ANIMATED_MATERIAL,
    RG_ID_ISNT_UNIQUE,
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

    // If true, acceleration structures related to skybox won't be built,
    // sky type RG_SKY_TYPE_GEOMETRY will be reset to RG_SKY_TYPE_COLOR.
    RgBool32                    disableGeometrySkybox;

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
    RG_GEOMETRY_PASS_THROUGH_TYPE_REFLECT
} RgGeometryPassThroughType;

typedef enum RgGeometryPrimaryVisibilityType
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

typedef struct RgMatrix
{
    float       matrix[4][4];
} RgMatrix;

typedef struct RgFloat3D
{
    float       data[3];
} RgFloat3D;

typedef struct RgFloat4D
{
    float       data[4];
} RgFloat4D;

typedef struct RgGeometryUploadInfo
{
    uint64_t                        uniqueID;

    RgGeometryType                  geomType;
    RgGeometryPassThroughType       passThroughType;
    RgGeometryPrimaryVisibilityType visibilityType;

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
    RgFloat4D                       layerColors[3];
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
    uint64_t        movableStaticUniqueID;
    RgTransform     transform;
} RgUpdateTransformInfo;

typedef struct RgUpdateTexCoordsInfo
{
    // movable or non-movable static unique geom ID
    uint64_t        staticUniqueID;
    uint32_t        vertexOffset;
    uint32_t        vertexCount;
    // If an array member is null, then texture coordinates
    // won't be updated for that layer.
    void            *texCoordLayerData[3];
} RgUpdateTexCoordsInfo;


// Uploaded static geometries are only visible after submitting them using rgSubmitStaticGeometries.
// Uploaded dynamic geometries are only visible in the current frame.
// "result" may be null, if its transform won't be changed
RgResult rgUploadGeometry(
    RgInstance                              rgInstance,
    const RgGeometryUploadInfo              *uploadInfo);

// Updating transform is available only for movable static geometry.
// Other geometry types don't need it because they are either fully static
// or uploaded every frame, so transforms are always as they are intended.
RgResult rgUpdateGeometryTransform(
    RgInstance                              rgInstance,
    const RgUpdateTransformInfo             *updateInfo);

RgResult rgUpdateGeometryTexCoords(
    RgInstance                              rgInstance,
    const RgUpdateTexCoordsInfo             *updateInfo);



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

typedef struct RgRasterizedGeometryVertexArrays
{
    // 3 first floats are used.
    void                *vertexData;
    // 2 first floats are used.
    void                *texCoordData;
    // RGBA packed into 32-bit uint. Little-endian. Can be null.
    void                *colorData;
    uint32_t            vertexStride;
    uint32_t            texCoordStride;
    uint32_t            colorStride;
} RgRasterizedGeometryVertexArrays;

typedef struct RgRasterizedGeometryVertexStruct
{
    float               position[3];
    // RGBA packed into 32-bit uint. Little-endian.
    uint32_t            packedColor;
    float               texCoord[2];
} RgRasterizedGeometryVertexStruct;

typedef struct RgRasterizedGeometryUploadInfo
{
    uint32_t            vertexCount;
    // Exactly one must be not null.
    // "arrays"  -- pointer to a struct that defines separate arrays
    //              for position and texCoord data.
    // "structs" -- is an array of packed vertices.
    RgRasterizedGeometryVertexArrays   *arrays;
    RgRasterizedGeometryVertexStruct   *structs;
    
    // Can be 0/null.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t            indexCount;
    void                *indexData;

    RgTransform         transform;

    RgFloat4D           color;
    // Only the albedo-alpha texture is used for rasterized geometry.
    RgMaterial          material;
    RgBool32            blendEnable;
    RgBlendFactor       blendFuncSrc;
    RgBlendFactor       blendFuncDst;
    RgBool32            depthTest;
    RgBool32            depthWrite;
    // If false, the rendering will be done with the resolution
    // (renderWidth, renderHeight) that is set in RgDrawFrameInfo.
    // Otherwise, swapchain's resolution will be used.
    // Note: if true, "depthTest" and "depthWrite" must be false.
    // Examples for "true": particles, semitransparent world objects
    // Examples for "false": HUD
    RgBool32            renderToSwapchain;
} RgRasterizedGeometryUploadInfo;



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
// x, y, width, height are specified in pixels. (x,y) defines top-left corner.
typedef struct RgViewport
{
    float       x;
    float       y;
    float       width;
    float       height;
    float       minDepth;
    float       maxDepth;
} RgViewport;

// Upload geometry that will be drawn using rasterization.
// Whole buffer for such geometry be discarded after frame finish.
// "viewProjection" -- 4x4 view-projection matrix to apply to the rasterized
//                     geometry. Matrix is column major. If it's null,
//                     then the matrices from RgDrawFrameInfo are used.
// "viewport"       -- pointer to a viewport to draw in. If it's null,
//                     then the fullscreen one is used with minDepth 0.0
//                     and maxDepth 1.0.
RgResult rgUploadRasterizedGeometry(
    RgInstance                              rgInstance,
    const RgRasterizedGeometryUploadInfo    *uploadInfo,
    const float                             *viewProjection,
    const RgViewport                        *viewport);



typedef enum RgLightType
{
    RG_LIGHT_TYPE_STATIC,
    RG_LIGHT_TYPE_DYNAMIC
} RgLightType;

typedef struct RgDirectionalLightUploadInfo
{
    uint64_t        uniqueID;
    RgLightType     type;
    RgFloat3D       color;
    RgFloat3D       direction;
    float           angularDiameterDegrees;
} RgDirectionalLightUploadInfo;

typedef struct RgSphericalLightUploadInfo
{
    uint64_t        uniqueID;
    RgLightType     type;
    RgFloat3D       color;
    RgFloat3D       position;
    // Sphere radius
    float           radius;
    // There will be no light after this distance
    float           falloffDistance;
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

typedef struct RgTextureData
{
    const void              *albedoAlphaData;
    const void              *normalsMetallicityData;
    const void              *emissionRoughnessData;
} RgTextureData;

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
    RgTextureData           textureData;
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
    RgTextureData           textureData;
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
    RgTextureData           textureData;
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



typedef struct RgCubemapFaceData
{
    const void *positiveX;
    const void *negativeX;
    const void *positiveY;
    const void *negativeY;
    const void *positiveZ;
    const void *negativeZ;
} RgCubemapFaceData;

typedef struct RgCubemapFacePaths
{
    const char *positiveX;
    const char *negativeX;
    const char *positiveY;
    const char *negativeY;
    const char *positiveZ;
    const char *negativeZ;
} RgCubemapFacePaths;

typedef struct RgCubemapCreateInfo
{
    union
    {
        const void          *data[6];
        RgCubemapFaceData   dataFaces;
    };

    // Overriding paths for each cubemap face.
    union
    {
        const char          *relativePaths[6];
        RgCubemapFacePaths  relativePathFaces;
    };

    // width = height = sideSize
    uint32_t                sideSize;
    RgBool32                useMipmaps;
    RgBool32                isSRGB;
    RgBool32                disableOverride;
    RgSamplerFilter         filter;
} RgCubemapCreateInfo;

RgResult rgCreateCubemap(
    RgInstance                          rgInstance,
    const RgCubemapCreateInfo           *createInfo,
    RgCubemap                           *result);

RgResult rgDestroyCubemap(
    RgInstance                          rgInstance,
    RgCubemap                           cubemap);



RgResult rgStartFrame(
    RgInstance                          rgInstance,
    uint32_t                            surfaceWidth, 
    uint32_t                            surfaceHeight, 
    RgBool32                            vsync, 
    RgBool32                            reloadShaders);

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
    RgSkyType           skyType;
    // Used as a main color for RG_SKY_TYPE_COLOR and ray-miss color for RG_SKY_TYPE_GEOMETRY.
    RgFloat3D           skyColorDefault;
    // The result sky color is multiplied by this value.
    float               skyColorMultiplier;
    // A point from which rays are traced while using RG_SKY_TYPE_GEOMETRY.
    RgFloat3D           skyViewerPosition;
    // If sky type is RG_SKY_TYPE_CUBEMAP, this cubemap is used.
    RgCubemap           skyCubemap;
    RgBool32            dbgShowMotionVectors;
    RgBool32            dbgShowGradients;
    
} RgDrawFrameInfo;

RgResult rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *frameInfo);

#ifdef __cplusplus
}
#endif