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

#ifndef RTGL1_H_
#define RTGL1_H_

#include <stdint.h>

#ifdef RG_USE_SURFACE_WIN32
    #include <windows.h>
#endif // RG_USE_SURFACE_WIN32
#ifdef RG_USE_SURFACE_METAL
    #ifdef __OBJC__
    @class CAMetalLayer;
    #else
    typedef void CAMetalLayer;
    #endif
#endif // RG_USE_SURFACE_METAL
#ifdef RG_USE_SURFACE_WAYLAND
    #include <wayland-client.h>
#endif // RG_USE_SURFACE_WAYLAND
#ifdef RG_USE_SURFACE_XCB
    #include <xcb/xcb.h>
#endif // RG_USE_SURFACE_XCB
#ifdef RG_USE_SURFACE_XLIB
    #include <X11/Xlib.h>
#endif // RG_USE_SURFACE_XLIB

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
RG_DEFINE_NON_DISPATCHABLE_HANDLE(RgInstance)
typedef uint32_t RgMaterial;
typedef uint32_t RgCubemap;
typedef uint32_t RgFlags;

#define RG_NULL_HANDLE      0
#define RG_NO_MATERIAL      0
#define RG_EMPTY_CUBEMAP    0
#define RG_FALSE            0
#define RG_TRUE             1

typedef enum RgResult
{
    RG_SUCCESS,
    RG_GRAPHICS_API_ERROR,
    RG_CANT_FIND_PHYSICAL_DEVICE,
    RG_WRONG_ARGUMENT,
    RG_TOO_MANY_INSTANCES,
    RG_WRONG_INSTANCE,
    RG_FRAME_WASNT_STARTED,
    RG_FRAME_WASNT_ENDED,
    RG_CANT_UPDATE_TRANSFORM,
    RG_CANT_UPDATE_TEXCOORDS,
    RG_CANT_UPDATE_DYNAMIC_MATERIAL,
    RG_CANT_UPDATE_ANIMATED_MATERIAL,
    RG_CANT_UPLOAD_RASTERIZED_GEOMETRY,
    RG_WRONG_MATERIAL_PARAMETER,
} RgResult;

typedef void (*PFN_rgPrint)(const char *pMessage, void *pUserData);
typedef void (*PFN_rgOpenFile)(const char *pFilePath, void *pUserData, const void **ppOutData, uint32_t *pOutDataSize, void **ppOutFileUserHandle);
typedef void (*PFN_rgCloseFile)(void *pFileUserHandle, void *pUserData);

typedef struct RgWin32SurfaceCreateInfo RgWin32SurfaceCreateInfo;
typedef struct RgMetalSurfaceCreateInfo RgMetalSurfaceCreateInfo;
typedef struct RgWaylandSurfaceCreateInfo RgWaylandSurfaceCreateInfo;
typedef struct RgXcbSurfaceCreateInfo RgXcbSurfaceCreateInfo;
typedef struct RgXlibSurfaceCreateInfo RgXlibSurfaceCreateInfo;

#ifdef RG_USE_SURFACE_WIN32
typedef struct RgWin32SurfaceCreateInfo
{
    HINSTANCE           hinstance;
    HWND                hwnd;
} RgWin32SurfaceCreateInfo;
#endif // RG_USE_SURFACE_WIN32

#ifdef RG_USE_SURFACE_METAL
typedef struct RgMetalSurfaceCreateInfo
{
    const CAMetalLayer  *pLayer;
} RgMetalSurfaceCreateInfo;
#endif // RG_USE_SURFACE_METAL

#ifdef RG_USE_SURFACE_WAYLAND
typedef struct RgWaylandSurfaceCreateInfo
{
    struct wl_display   *display;
    struct wl_surface   *surface;
} RgWaylandSurfaceCreateInfo;
#endif // RG_USE_SURFACE_WAYLAND

#ifdef RG_USE_SURFACE_XCB
typedef struct RgXcbSurfaceCreateInfo
{
    xcb_connection_t    *connection;
    xcb_window_t        window;
} RgXcbSurfaceCreateInfo;
#endif // RG_USE_SURFACE_XCB

#ifdef RG_USE_SURFACE_XLIB
typedef struct RgXlibSurfaceCreateInfo
{
    Display             *dpy;
    Window              window;
} RgXlibSurfaceCreateInfo;
#endif // RG_USE_SURFACE_XLIB

typedef struct RgInstanceCreateInfo
{
    const char                  *pName;

    // Exactly one of these surface create infos must be not null.
    RgWin32SurfaceCreateInfo    *pWin32SurfaceInfo;
    RgMetalSurfaceCreateInfo    *pMetalSurfaceCreateInfo;
    RgWaylandSurfaceCreateInfo  *pWaylandSurfaceCreateInfo;
    RgXcbSurfaceCreateInfo      *pXcbSurfaceCreateInfo;
    RgXlibSurfaceCreateInfo     *pXlibSurfaceCreateInfo;

    RgBool32                    enableValidationLayer;
    // Optional function to print messages from the library.
    PFN_rgPrint                 pfnPrint;
    // Custom user data that is passed to pfnUserPrint.
    void                        *pUserPrintData;

    const char                  *pShaderFolderPath;
    // Path to the file with 128 layers of uncompressed 128x128 blue noise images.
    const char                  *pBlueNoiseFilePath;
    // Optional function to load files: shaders, blue noise and overriden textures.
    // If null, files will be opened with standard methods. pfnLoadFile is very simple,
    // as it requires file data (ppOutData, pOutDataSize) to be fully loaded to the memory.
    // The value, ppOutFileUserHandle point on, will be passed to PFN_rgCloseFile.
    // So for example, it can be a file handle.
    PFN_rgOpenFile              pfnOpenFile;
    PFN_rgCloseFile             pfnCloseFile;
    // Custom user data that is passed to pfnUserLoadFile.
    void                        *pUserLoadFileData;

    // How many texture layers should be used to get albedo color for primary rays / indrect illumination.
    uint32_t                    primaryRaysMaxAlbedoLayers;
    uint32_t                    indirectIlluminationMaxAlbedoLayers;

    // Memory that must be allocated for vertex and index buffers of rasterized geometry.
    // It can't be changed after rgCreateInstance.
    // If buffer is full, rasterized data will be ignored
    uint32_t                    rasterizedMaxVertexCount;
    uint32_t                    rasterizedMaxIndexCount;
    // Apply gamma correction to packed rasterized vertex colors.
    RgBool32                    rasterizedVertexColorGamma;
    uint32_t                    rasterizedSkyMaxVertexCount;
    uint32_t                    rasterizedSkyMaxIndexCount;

    // Size of a cubemap side to render rasterized sky in.
    uint32_t                    rasterizedSkyCubemapSize;  

    // The folder to find overriding textures in.
    const char                  *pOverridenTexturesFolderPath;
    // Postfixes will be used to determine textures that should be 
    // loaded from files if the texture should be overridden
    // i.e. if postfix="_n" then "Floor_01.*" => "Floor_01_n.*", 
    // where "*" is some image extension
    // If null, then empty string will be used.
    const char                  *pOverridenAlbedoAlphaTexturePostfix;
    // If null, then "_rme" will be used.
    const char                  *pOverridenRoughnessMetallicEmissionTexturePostfix;
    // If null, then "_n" will be used.
    const char                  *pOverridenNormalTexturePostfix;
    // Overriden textures 
    RgBool32                    overridenAlbedoAlphaTextureIsSRGB;
    RgBool32                    overridenRoughnessMetallicEmissionTextureIsSRGB;
    RgBool32                    overridenNormalTextureIsSRGB;

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
    const RgInstanceCreateInfo          *pInfo,
    RgInstance                          *pResult);

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

// WORLD_0, WORLD_1, WORLD_2 may be specified for culling with RgDrawFrameInfo::rayCullMaskWorld
typedef enum RgGeometryPrimaryVisibilityType
{
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_0,
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_1,
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER,
} RgGeometryPrimaryVisibilityType;

typedef enum RgGeometryMaterialBlendType
{
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_OPAQUE,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_ALPHA,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_ADD,
    RG_GEOMETRY_MATERIAL_BLEND_TYPE_SHADE
} RgGeometryMaterialBlendType;

// Row-major transformation matrix.
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

typedef enum RgGeometryUploadFlagBits
{
    RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT = 0x00000001,
} RgGeometryUploadFlagBits;
typedef RgFlags RgGeometryUploadFlags;

typedef struct RgGeometryUploadInfo
{
    uint64_t                        uniqueID;
    RgGeometryUploadFlags           flags;

    RgGeometryType                  geomType;
    RgGeometryPassThroughType       passThroughType;
    RgGeometryPrimaryVisibilityType visibilityType;

    uint32_t                        vertexCount;
    // Strides are set in RgInstanceUploadInfo.
    // 3 first floats will be used
    const void                      *pVertexData;
    // 3 first floats will be used
    // If null, then the normals will be generated.
    // If null and RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT is set,
    // generated normals will be inverted.
    const void                      *pNormalData;
    // Up to 3 texture coordinated per vertex for static geometry.
    // Dynamic geometry uses only 1 layer.
    // 2 first floats will be used
    const void                      *pTexCoordLayerData[3];
    
    // Can be null, if indices are not used.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t                        indexCount;
    const void                      *pIndexData;

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
    const void      *pTexCoordLayerData[3];
} RgUpdateTexCoordsInfo;


// Uploaded static geometries are only visible after submitting them using rgSubmitStaticGeometries.
// Uploaded dynamic geometries are only visible in the current frame.
// "result" may be null, if its transform won't be changed
RgResult rgUploadGeometry(
    RgInstance                              rgInstance,
    const RgGeometryUploadInfo              *pUploadInfo);

// Updating transform is available only for movable static geometry.
// Other geometry types don't need it because they are either fully static
// or uploaded every frame, so transforms are always as they are intended.
RgResult rgUpdateGeometryTransform(
    RgInstance                              rgInstance,
    const RgUpdateTransformInfo             *pUpdateInfo);

RgResult rgUpdateGeometryTexCoords(
    RgInstance                              rgInstance,
    const RgUpdateTexCoordsInfo             *pUpdateInfo);



typedef enum RgBlendFactor
{
    RG_BLEND_FACTOR_ONE,
    RG_BLEND_FACTOR_ZERO,
    RG_BLEND_FACTOR_SRC_COLOR,
    RG_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    RG_BLEND_FACTOR_DST_COLOR,
    RG_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    RG_BLEND_FACTOR_SRC_ALPHA,
    RG_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
} RgBlendFactor;

// DEFAULT:     The rendering will be done with the resolution
//              (renderWidth, renderHeight) that is set in RgDrawFrameInfo.
//              Examples: particles, semitransparent world objects.
// SWAPCHAIN:   Swapchain's resolution will be used.
//              Note: "depthTest" and "depthWrite" must be false.
//              Examples: HUD
// SKY:         Geometry will be drawn to the background of ray-traced image
//              if skyType is RG_SKY_TYPE_RASTERIZED_GEOMETRY.
//              Also, the cubemap for this kind of geometry will be created
//              for specular and indirect bounces.
typedef enum RgRaterizedGeometryRenderType
{
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT,
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN,
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY
} RgRaterizedGeometryRenderType;

typedef struct RgRasterizedGeometryVertexArrays
{
    // 3 first floats are used.
    const void          *pVertexData;
    // 2 first floats are used. Can be null.
    const void          *pTexCoordData;
    // RGBA packed into 32-bit uint. Little-endian. Can be null.
    const void          *pColorData;
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
    RgRaterizedGeometryRenderType renderType;

    uint32_t            vertexCount;
    // Exactly one must be not null.
    // "pArrays"  -- pointer to a struct that defines separate arrays
    //               for position and texCoord data.
    // "pStructs" -- is an array of packed vertices.
    const RgRasterizedGeometryVertexArrays *pArrays;
    const RgRasterizedGeometryVertexStruct *pStructs;
    
    // Can be 0/null.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t            indexCount;
    const void          *pIndexData;

    RgTransform         transform;

    RgFloat4D           color;
    // Only the albedo-alpha texture is used for rasterized geometry.
    RgMaterial          material;
    RgBool32            blendEnable;
    RgBlendFactor       blendFuncSrc;
    RgBlendFactor       blendFuncDst;
    RgBool32            depthTest;
    RgBool32            depthWrite;
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
    const RgRasterizedGeometryUploadInfo    *pUploadInfo,
    const float                             *pViewProjection,
    const RgViewport                        *pViewport);



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

// Only one spotlight is available in a scene.
// That instance is specified throw RgDrawFrameInfo.
typedef struct RgSpotlightUploadInfo
{
    RgFloat3D position;
    RgFloat3D direction;
    RgFloat3D upVector;
    RgFloat3D color;
    // Light source disk radius.
    float radius;
    // Inner cone angle. In radians.
    float angleOuter;
    // Outer cone angle. In radians.
    float angleInner;
    // Distance at which light intensity is zero.
    float falloffDistance;
} RgSpotlightUploadInfo;

RgResult rgUploadDirectionalLight(
    RgInstance                          rgInstance,
    RgDirectionalLightUploadInfo        *pLightInfo);

RgResult rgUploadSphericalLight(
    RgInstance                          rgInstance,
    RgSphericalLightUploadInfo          *pLightInfo);

RgResult rgUploadSpotlightLight(
    RgInstance                          rgInstance,
    RgSpotlightUploadInfo               *pLightInfo);



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
    const void              *pData;
    RgBool32                isSRGB;
} RgTextureData;

typedef struct RgTextureSet
{
    RgTextureData albedoAlpha;
    RgTextureData roughnessMetallicEmission;
    RgTextureData normal;
} RgTextureSet;

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
    RgTextureSet            textures;
    // The library will try to find image files using path:
    //      "<overridenTexturesFolderPath>/<relativePath>".
    // "relativePath" must be in the following format:
    //      "<folders>/<name>.<extension>".
    // "name" will be used as a base for overriding texture names.
    // "folders" and "extension" are not used.
    // Image files must be in KTX2 format.
    const char              *pRelativePath;
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
    RgTextureSet            textures;
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
    RgTextureSet            textures;
} RgDynamicMaterialUpdateInfo;

typedef struct RgAnimatedMaterialCreateInfo
{
    uint32_t                            frameCount;
    RgStaticMaterialCreateInfo          *pFrames;
} RgAnimatedMaterialCreateInfo;

RgResult rgCreateStaticMaterial(
    RgInstance                          rgInstance,
    const RgStaticMaterialCreateInfo    *pCreateInfo,
    RgMaterial                          *pResult);

RgResult rgCreateAnimatedMaterial(
    RgInstance                          rgInstance,
    const RgAnimatedMaterialCreateInfo  *pCreateInfo,
    RgMaterial                          *pResult);

RgResult rgChangeAnimatedMaterialFrame(
    RgInstance                          rgInstance,
    RgMaterial                          animatedMaterial,
    uint32_t                            frameIndex);
    
RgResult rgCreateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialCreateInfo   *pCreateInfo,
    RgMaterial                          *pResult);

RgResult rgUpdateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialUpdateInfo   *pUpdateInfo);

// Destroying RG_NO_MATERIAL has no effect.
RgResult rgDestroyMaterial(
    RgInstance                          rgInstance,
    RgMaterial                          material);



typedef struct RgCubemapFaceData
{
    const void *pPositiveX;
    const void *pNegativeX;
    const void *pPositiveY;
    const void *pNegativeY;
    const void *pPositiveZ;
    const void *pNegativeZ;
} RgCubemapFaceData;

typedef struct RgCubemapFacePaths
{
    const char *pPositiveX;
    const char *pNegativeX;
    const char *pPositiveY;
    const char *pNegativeY;
    const char *pPositiveZ;
    const char *pNegativeZ;
} RgCubemapFacePaths;

typedef struct RgCubemapCreateInfo
{
    union
    {
        const void          *pData[6];
        RgCubemapFaceData   dataFaces;
    };

    // Overriding paths for each cubemap face.
    union
    {
        const char          *pRelativePaths[6];
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
    const RgCubemapCreateInfo           *pCreateInfo,
    RgCubemap                           *pResult);

RgResult rgDestroyCubemap(
    RgInstance                          rgInstance,
    RgCubemap                           cubemap);



typedef struct RgStartFrameInfo
{
    RgExtent2D      surfaceSize;
    RgBool32        requestVSync;
    RgBool32        requestShaderReload;
    // Reuse sky geometry from the previous frames.
    // The rasterized skybox cubemap won't be rerendered.
    RgBool32        requestRasterizedSkyGeometryReuse;
} RgStartFrameInfo;

RgResult rgStartFrame(
    RgInstance                          rgInstance,
    const RgStartFrameInfo              *pStartInfo);

typedef enum RgSkyType
{
    RG_SKY_TYPE_COLOR,
    RG_SKY_TYPE_CUBEMAP,
    RG_SKY_TYPE_RASTERIZED_GEOMETRY
} RgSkyType;

typedef struct RgDrawFrameTonemappingParams
{
    float               minLogLuminance;
    float               maxLogLuminance;
    float               luminanceWhitePoint;
} RgDrawFrameTonemappingParams;

typedef struct RgDrawFrameSkyParams
{
    RgSkyType           skyType;
    // Used as a main color for RG_SKY_TYPE_COLOR and ray-miss color for RG_SKY_TYPE_GEOMETRY.
    RgFloat3D           skyColorDefault;
    // The result sky color is multiplied by this value.
    float               skyColorMultiplier;
    float               skyColorSaturation;
    // A point from which rays are traced while using RG_SKY_TYPE_GEOMETRY.
    RgFloat3D           skyViewerPosition;
    // If sky type is RG_SKY_TYPE_CUBEMAP, this cubemap is used.
    RgCubemap           skyCubemap;
} RgDrawFrameSkyParams;

typedef struct RgDrawFrameOverridenTexturesParams
{
    float               normalMapStrength;
    // Multiplier for emission map values.
    float               emissionMapBoost;
    // Upper bound for emissive materials in primary albedo channel (i.e. on screen).
    float               emissionMaxScreenColor;
} RgDrawFrameOverridenTexturesParams;

typedef struct RgDrawFrameDebugParams
{
    RgBool32            showMotionVectors;
    RgBool32            showGradients;
} RgDrawFrameDebugParams;

typedef struct RgDrawFrameShadowParams
{
    // Directional light shadow rays are cast, if illumination bounce index is in [0, maxBounceShadowsSphereLights).
    uint32_t            maxBounceShadowsDirectionalLights;
    // Sphere light shadow rays are cast, if illumination bounce index is in [0, maxBounceShadowsDirectionalLights).
    uint32_t            maxBounceShadowsSphereLights;
    // Spotlight shadow rays are cast, if illumination bounce index is in [0, maxBounceShadowsSpotlights).
    uint32_t            maxBounceShadowsSpotlights;
} RgDrawFrameShadowParams;

typedef struct RgDrawFrameInfo
{
    // View and projection matrices are column major.
    float               view[16];
    float               projection[16];
    // What world parts to render. Mask bits are in [0..0x7]
    // Affects only geometry with RG_GEOMETRY_VISIBILITY_TYPE_WORLD_*
    uint32_t            rayCullMaskWorld;
    RgBool32            disableRayTracing;
    RgBool32            disableRasterization;
    RgExtent2D          renderSize;
    double              currentTime;
    RgBool32            disableEyeAdaptation;

    // Null, if default values should be used.
    const RgDrawFrameShadowParams               *pShadowParams;
    const RgDrawFrameTonemappingParams          *pTonemappingParams;
    const RgDrawFrameSkyParams                  *pSkyParams;
    const RgDrawFrameOverridenTexturesParams    *pOverridenTexturesParams;
    const RgDrawFrameDebugParams                *pDebugParams;

} RgDrawFrameInfo;

RgResult rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *pDrawInfo);

#ifdef __cplusplus
}
#endif

#endif // RTGL1_H_
