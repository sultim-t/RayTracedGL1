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

#if defined(_WIN32)
    #ifdef RG_LIBRARY_EXPORTS
        #define RGAPI __declspec(dllexport)
    #else
        #define RGAPI __declspec(dllimport)
    #endif // RTGL1_EXPORTS
    #define RGCONV __cdecl
#else
    #define RGAPI
    #define RGCONV
#endif // defined(_WIN32)

#define RG_RTGL_VERSION_API "1.03.0000"

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
    RG_WRONG_FUNCTION_CALL,
    RG_TOO_MANY_SECTORS,
    RG_ERROR_INCORRECT_SECTOR,
    RG_ERROR_CANT_FIND_BLUE_NOISE,
    RG_ERROR_CANT_FIND_WATER_TEXTURES,
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
    // Application name.
    const char                  *pAppName;
    // Application GUID. Generate it for your application and specify it here.
    const char                  *pAppGUID;

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

    // If true, raw .png files can be loaded.
    RgBool32                    isDeveloperMode;

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

    RgBool32                    rayCullBackFacingTriangles;
    // Allow RG_GEOMETRY_VISIBILITY_TYPE_SKY.
    // If true, RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2 must not be used.
    RgBool32                    allowGeometryWithSkyFlag;

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

    // Max amount of textures to be used during the execution.
    // The value is clamped to [1024..4096]
    uint32_t                    maxTextureCount;
    // If true, 'filter' in RgStaticMaterialCreateInfo, RgDynamicMaterialCreateInfo, RgCubemapCreateInfo
    // will set only magnification filter.
    RgBool32                    textureSamplerForceMinificationFilterLinear;

    // The folder to find overriding textures in. Must contain '/' at the end.
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

    RgBool32                    originalAlbedoAlphaTextureIsSRGB;
    RgBool32                    originalRoughnessMetallicEmissionTextureIsSRGB;
    RgBool32                    originalNormalTextureIsSRGB;

    RgBool32                    overridenAlbedoAlphaTextureIsSRGB;
    RgBool32                    overridenRoughnessMetallicEmissionTextureIsSRGB;
    RgBool32                    overridenNormalTextureIsSRGB;

    // Path to normal texture path. Ignores pOverridenTexturesFolderPath and pOverridenNormalTexturePostfix
    const char                  *pWaterNormalTexturePath;

    RgBool32                    lensFlareVerticesInScreenSpace;
    // If true, 'pointToCheck' XY are screen space [0..1] coordinates to check NDC depth [0..1] which is specified in Z.
    // Otherwise, XYZ specify a world point to which view-projection will be applied to determine its
    // screen space coords and NDC depth.
    RgBool32                    lensFlarePointToCheckIsInScreenSpace;

} RgInstanceCreateInfo;

RGAPI RgResult RGCONV rgCreateInstance(
    const RgInstanceCreateInfo          *pInfo,
    RgInstance                          *pResult);

RGAPI RgResult RGCONV rgDestroyInstance(
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
    RG_GEOMETRY_PASS_THROUGH_TYPE_MIRROR,
    RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL,
    RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_ONLY_REFLECT,
    RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_REFLECT_REFRACT,
    RG_GEOMETRY_PASS_THROUGH_TYPE_GLASS_REFLECT_REFRACT,
} RgGeometryPassThroughType;

typedef enum RgGeometryPrimaryVisibilityType
{
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_0,
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_1,
    RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON,
    RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER,
    // If ray hits this geometry, then pretend like it was a miss (i.e. fetch sky info)
    RG_GEOMETRY_VISIBILITY_TYPE_SKY,
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

typedef struct RgMatrix3D
{
    float       matrix[3][3];
} RgMatrix3D;

typedef struct RgFloat2D
{
    float       data[2];
} RgFloat2D;

typedef struct RgFloat3D
{
    float       data[3];
} RgFloat3D;

typedef struct RgFloat4D
{
    float       data[4];
} RgFloat4D;

typedef struct RgVertex
{
    float       position[3];        uint32_t _padding0;
    float       normal[3];          uint32_t _padding1;
    float       texCoord[2];
    float       texCoordLayer1[2];
    float       texCoordLayer2[2];
    // RGBA packed into 32-bit uint. R component is at the little end, i.e. (a<<24 | b<<16 | g<<8 | r)
    uint32_t    packedColor;        uint32_t _padding2;
} RgVertex;

typedef enum RgGeometryUploadFlagBits
{
    RG_GEOMETRY_UPLOAD_GENERATE_NORMALS_BIT = 1,
    RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT = 2,
    // Set this flag if on the both sides of polygons the media is the same.
    // For example, waterfall geometry represented by one flat square,
    // so on both sides is air media.
    RG_GEOMETRY_UPLOAD_NO_MEDIA_CHANGE_ON_REFRACT_BIT = 4,
    // Multiply the thoughput by albedo on reflection / refraction.
    // E.g. mirror has some texture on it. 
    RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_MULTIPLY_BIT = 8,
    RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_ADD_BIT = 16,
    // Ignore refl/refr geometry after one refl/refr hit.
    RG_GEOMETRY_UPLOAD_IGNORE_REFL_REFR_AFTER_ONE_REFL_REFR_BIT = 32,
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
    const RgVertex                  *pVertices;
    
    // Can be null, if indices are not used.
    // pIndices is an array of uint32_t of size indexCount.
    uint32_t                        indexCount;
    const uint32_t                  *pIndices;

    // Sector ID per triangle. If null, sector IDs are ignored.
    // Otherwise, must point to an array of (vertexCount/3) or
    // (indexCount/3) elements (if pIndices is not null)
    const uint32_t                  *pTriangleSectorIDs;
    // If per triangle sector IDs are not provided,
    // use this for whole geometry.
    uint32_t                        sectorID;

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


// Uploaded dynamic geometry can only be visible in the current frame, i.e.
// dynamic geometry must be uploaded each frame.
// Uploaded static geometriy can only be visible after submitting them using rgSubmitStaticGeometries.
// Dynamic geometry can be uploaded only between rgStartFrame - rgDrawFrame.
// Static geometry can be uploaded only between rgBeginStaticGeometries - rgSubmitStaticGeometries.
// Uploading dynamic geometries and then calling rgBeginStaticGeometries will erase them.
RGAPI RgResult RGCONV rgUploadGeometry(
    RgInstance                              rgInstance,
    const RgGeometryUploadInfo              *pUploadInfo);

// Updating transform is available only for movable static geometry.
// Other geometry types don't need it because they are either fully static
// or uploaded every frame, so transforms are always as they are intended.
RGAPI RgResult RGCONV rgUpdateGeometryTransform(
    RgInstance                              rgInstance,
    const RgUpdateTransformInfo             *pUpdateInfo);

RGAPI RgResult RGCONV rgUpdateGeometryTexCoords(
    RgInstance                              rgInstance,
    const RgUpdateTexCoordsInfo             *pUpdateInfo);



// Clear current scene from all static geometries and make it available for recording new geometries.
// New scene can be visible only after the submission using rgSubmitStaticGeometries.
RGAPI RgResult RGCONV rgBeginStaticGeometries(
    RgInstance                          rgInstance);

// After uploading all static geometry, scene must be submitted before rendering.
// Note that movable static geometry can be still moved using rgUpdateGeometryTransform.
// If the static scene geometry should be changed, it must be cleared using rgBeginStaticGeometries
// and new static geometries must be uploaded.
// To clear static scene, call rgBeginStaticGeometries and then rgSubmitStaticGeometries
// without uploading any geometry.
// rgBeginStaticGeometries and rgSubmitStaticGeometries can be called outside of rgStartFrame-rgDrawFrame.
RGAPI RgResult RGCONV rgSubmitStaticGeometries(
    RgInstance                          rgInstance);



// Set mutual potential visibility between sectors A and B.
// It improves the light sampling by using specific light lists for each sector.
// If none was set, light sources are chosen uniformly.
// Note: visibility data is being cleared after calling rgBeginStaticGeometries.
// Note: sector can be registered by just specifiying the same value for sectorID_A and sectorID_B.
RGAPI RgResult RGCONV rgSetPotentialVisibility(
    RgInstance                          rgInstance,
    uint32_t                            sectorID_A,
    uint32_t                            sectorID_B);



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
typedef enum RgRasterizedGeometryRenderType
{
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT,
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN,
    RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY
} RgRasterizedGeometryRenderType;

typedef enum RgRasterizedGeometryStateFlagBits
{
    RG_RASTERIZED_GEOMETRY_STATE_ALPHA_TEST         = 1,
    RG_RASTERIZED_GEOMETRY_STATE_BLEND_ENABLE       = 2,
    RG_RASTERIZED_GEOMETRY_STATE_DEPTH_TEST         = 4,
    RG_RASTERIZED_GEOMETRY_STATE_DEPTH_WRITE        = 8,
    RG_RASTERIZED_GEOMETRY_STATE_FORCE_LINE_LIST    = 16,
} RgRasterizedGeometryStateFlagBits;
typedef uint32_t RgRasterizedGeometryStateFlags;

typedef struct RgRasterizedGeometryUploadInfo
{
    RgRasterizedGeometryRenderType          renderType;

    uint32_t                                vertexCount;
    const RgVertex                          *pVertices;
    
    // Can be 0/null.
    // indexData is an array of uint32_t of size indexCount.
    uint32_t                                indexCount;
    const void                              *pIndices;

    RgTransform                             transform;

    RgFloat4D                               color;
    // Only the albedo-alpha texture is used for rasterized geometry.
    RgMaterial                              material;
    RgRasterizedGeometryStateFlags          pipelineState;
    RgBlendFactor                           blendFuncSrc;
    RgBlendFactor                           blendFuncDst;
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
RGAPI RgResult RGCONV rgUploadRasterizedGeometry(
    RgInstance                              rgInstance,
    const RgRasterizedGeometryUploadInfo    *pUploadInfo,
    const float                             *pViewProjection,
    const RgViewport                        *pViewport);



// Render specified vertex geometry, if 'pointToCheck' is not hidden.
typedef struct RgLensFlareUploadInfo
{
    // Must be in world space.
    uint32_t                                vertexCount;
    const RgVertex                          *pVertices;
    // Must not be null.
    uint32_t                                indexCount;
    const uint32_t                          *pIndices;
    RgMaterial                              material;
    // Format is defined by 'lensFlarePointToCheckIsInScreenSpace'
    RgFloat3D                               pointToCheck;
} RgLensFlareUploadInfo;

RGAPI RgResult RGCONV rgUploadLensFlare(
    RgInstance                              rgInstance,
    const RgLensFlareUploadInfo             *pUploadInfo);



typedef struct RgDecalUploadInfo
{
    // Transformation from [-0.5, 0.5] cube to a scaled oriented box.
    // Orientation should transform (0,0,1) to decal's normal.
    RgTransform     transform;
    RgMaterial      material;
} RgDecalUploadInfo;

RGAPI RgResult RGCONV rgUploadDecal(
    RgInstance                              rgInstance,
    const RgDecalUploadInfo                 *pUploadInfo);



typedef struct RgDirectionalLightUploadInfo
{
    // Used to match the same light source from the previous frame.
    uint64_t        uniqueID;
    RgFloat3D       color;
    RgFloat3D       direction;
    float           angularDiameterDegrees;
} RgDirectionalLightUploadInfo;

typedef struct RgSphericalLightUploadInfo
{
    // Used to match the same light source from the previous frame.
    uint64_t        uniqueID;
    // Look notes in RgPolygonalLightUploadInfo::sectorID
    uint32_t        sectorID;
    RgFloat3D       color;
    RgFloat3D       position;
    // Sphere radius.
    float           radius;
} RgSphericalLightUploadInfo;

typedef struct RgPolygonalLightUploadInfo
{
    // Used to match the same light source from the previous frame.
    uint64_t        uniqueID;
    // ID of the sector this light belongs to. Can be any uint32_t value.
    // If advanced sampling technique is not needed, leave the field with 0,
    // so all lights will use that one sector, but more noisy results should be expected.
    uint32_t        sectorID;
    RgFloat3D       color;
    RgFloat3D       positions[3];
} RgPolygonalLightUploadInfo;

// Only one spotlight is available in a scene.
typedef struct RgSpotLightUploadInfo
{
    // Used to match the same light source from the previous frame.
    uint64_t        uniqueID;
    // Look notes in RgPolygonalLightUploadInfo::sectorID
    uint32_t        sectorID;
    RgFloat3D       color;
    RgFloat3D       position;
    RgFloat3D       direction;
    float           radius;
    // Light source disk radius.
    // Inner cone angle. In radians.
    float           angleOuter;
    // Outer cone angle. In radians.
    float           angleInner;
} RgSpotLightUploadInfo;

RGAPI RgResult RGCONV rgUploadDirectionalLight(
    RgInstance                          rgInstance,
    const RgDirectionalLightUploadInfo  *pUploadInfo);

RGAPI RgResult RGCONV rgUploadSphericalLight(
    RgInstance                          rgInstance,
    const RgSphericalLightUploadInfo    *pUploadInfo);

RGAPI RgResult RGCONV rgUploadSpotLight(
    RgInstance                          rgInstance,
    const RgSpotLightUploadInfo         *pUploadInfo);

RGAPI RgResult RGCONV rgUploadPolygonalLight(
    RgInstance                          rgInstance,
    const RgPolygonalLightUploadInfo    *pUploadInfo);



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

typedef struct RgTextureSet
{
    const void *pDataAlbedoAlpha;
    const void *pDataRoughnessMetallicEmission;
    const void *pDataNormal;
} RgTextureSet;

typedef enum RgMaterialCreateFlagBits
{
    // If set, mipmaps will be generated by the library.
    RG_MATERIAL_CREATE_DONT_GENERATE_MIPMAPS_BIT = 1,
    // Force to use lowest mip level while rendering
    RG_MATERIAL_CREATE_FORCE_LOWEST_MIP_BIT = 2,
    // If set, the library won't try to find files with additional info.
    // Always set for dynamic materials.
    RG_MATERIAL_CREATE_DISABLE_OVERRIDE_BIT = 4,
    // If set, sampler will be controlled with RgDrawFrameTexturesParams::dynamicSamplerFilter.
    RG_MATERIAL_CREATE_DYNAMIC_SAMPLER_FILTER_BIT = 8,
} RgMaterialCreateFlagBits;
typedef RgFlags RgMaterialCreateFlags;

typedef struct RgStaticMaterialCreateInfo
{
    RgMaterialCreateFlags   flags;
    // If data is used then size must specify width and height.
    // "data" must be (width * height * 4) bytes.
    RgExtent2D              size;
    // Only R8G8B8A8 textures.
    // Firstly, the library will try to find image file using "relativePath",
    // and if nothing is found "data" is used. Additional overriding data
    // such as normal, metallic, roughness, emission maps will be loaded
    // using "relativePath" and overriding postfixes.
    RgTextureSet            textures;
    // "relativePath" must be in the following format:
    //      "<folders>/<name>.<extension>"
    // where "<folders>/" and ".<extension>" can be empty.
    // The library will try to find image files using path:
    //      "<overridenTexturesFolderPath><folders>/<name>.ktx2"
    // Image files must be in KTX2 format.
    const char              *pRelativePath;
    RgSamplerFilter         filter;
    RgSamplerAddressMode    addressModeU;
    RgSamplerAddressMode    addressModeV;
} RgStaticMaterialCreateInfo;
    
typedef struct RgDynamicMaterialCreateInfo
{
    RgMaterialCreateFlags   flags;
    // The width and height must be > 0.
    RgExtent2D              size;
    // Only R8G8B8A8 textures.
    // If data is not null, the newly created dynamic texture will be
    // updated using this data. Otherwise, it'll be empty until
    // "rgUpdateDynamicMaterial" call.
    RgTextureSet            textures;
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

RGAPI RgResult RGCONV rgCreateStaticMaterial(
    RgInstance                          rgInstance,
    const RgStaticMaterialCreateInfo    *pCreateInfo,
    RgMaterial                          *pResult);

RGAPI RgResult RGCONV rgCreateAnimatedMaterial(
    RgInstance                          rgInstance,
    const RgAnimatedMaterialCreateInfo  *pCreateInfo,
    RgMaterial                          *pResult);

RGAPI RgResult RGCONV rgChangeAnimatedMaterialFrame(
    RgInstance                          rgInstance,
    RgMaterial                          animatedMaterial,
    uint32_t                            frameIndex);
    
RGAPI RgResult RGCONV rgCreateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialCreateInfo   *pCreateInfo,
    RgMaterial                          *pResult);

RGAPI RgResult RGCONV rgUpdateDynamicMaterial(
    RgInstance                          rgInstance,
    const RgDynamicMaterialUpdateInfo   *pUpdateInfo);

// Destroying RG_NO_MATERIAL has no effect.
RGAPI RgResult RGCONV rgDestroyMaterial(
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
    RgBool32                disableOverride;
    RgSamplerFilter         filter;
} RgCubemapCreateInfo;

RGAPI RgResult RGCONV rgCreateCubemap(
    RgInstance                          rgInstance,
    const RgCubemapCreateInfo           *pCreateInfo,
    RgCubemap                           *pResult);

RGAPI RgResult RGCONV rgDestroyCubemap(
    RgInstance                          rgInstance,
    RgCubemap                           cubemap);



typedef struct RgStartFrameInfo
{
    RgBool32        requestVSync;
    RgBool32        requestShaderReload;
    // Reuse sky geometry from the previous frames.
    // The rasterized skybox cubemap won't be rerendered.
    RgBool32        requestRasterizedSkyGeometryReuse;
} RgStartFrameInfo;

RGAPI RgResult RGCONV rgStartFrame(
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
    float       minLogLuminance;
    float       maxLogLuminance;
    float       luminanceWhitePoint;
} RgDrawFrameTonemappingParams;

typedef struct RgDrawFrameSkyParams
{
    RgSkyType   skyType;
    // Used as a main color for RG_SKY_TYPE_COLOR.
    RgFloat3D   skyColorDefault;
    // The result sky color is multiplied by this value.
    float       skyColorMultiplier;
    float       skyColorSaturation;
    // A point from which rays are traced while using RG_SKY_TYPE_RASTERIZED_GEOMETRY.
    RgFloat3D   skyViewerPosition;
    // If sky type is RG_SKY_TYPE_CUBEMAP, this cubemap is used.
    RgCubemap   skyCubemap;
    // Apply this transform to the direction when sampling a sky cubemap (RG_SKY_TYPE_CUBEMAP).
    // If equals to zero, then default value is used.
    // Default: identity matrix.
    RgMatrix3D  skyCubemapRotationTransform;
} RgDrawFrameSkyParams;

typedef struct RgDrawFrameTexturesParams
{
    // What sampler filter to use for materials with RG_MATERIAL_CREATE_DYNAMIC_SAMPLER_FILTER_BIT.
    // Should be changed infrequently, as it reloads all texture descriptors.
    RgSamplerFilter dynamicSamplerFilter;
    float           normalMapStrength;
    // Multiplier for emission map values for indirect lighting.
    float           emissionMapBoost;
    // Upper bound for emissive materials in primary albedo channel (i.e. on screen).
    float           emissionMaxScreenColor;
} RgDrawFrameTexturesParams;

typedef enum RgDebugDrawFlagBits
{
    RG_DEBUG_DRAW_ONLY_DIFFUSE_DIRECT_BIT = 1,
    RG_DEBUG_DRAW_ONLY_DIFFUSE_INDIRECT_BIT = 2,
    RG_DEBUG_DRAW_ONLY_SPECULAR_BIT = 4,
    RG_DEBUG_DRAW_UNFILTERED_DIFFUSE_DIRECT_BIT = 8,
    RG_DEBUG_DRAW_UNFILTERED_DIFFUSE_INDIRECT_BIT = 16,
    RG_DEBUG_DRAW_UNFILTERED_SPECULAR_BIT = 32,
    RG_DEBUG_DRAW_ALBEDO_WHITE_BIT = 64,
    RG_DEBUG_DRAW_MOTION_VECTORS_BIT = 128,
    RG_DEBUG_DRAW_GRADIENTS_BIT = 256,
    RG_DEBUG_DRAW_LIGHT_GRID_BIT = 512,
} RgDebugDrawFlagBits;
typedef RgFlags RgDebugDrawFlags;

typedef struct RgDrawFrameDebugParams
{
    RgDebugDrawFlags drawFlags;
} RgDrawFrameDebugParams;

typedef struct RgDrawFrameShadowParams
{
    // Shadow rays are cast, if illumination bounce index is in [0, maxBounceShadows).
    uint32_t    maxBounceShadows;
    // The higher the value, the more polygonal lights act like a spotlight. 
    // Default: 2
    float       polygonalLightSpotlightFactor;
    // Clamp indirect diffuse with this value to prevent fireflies.
    // Default: 3.0
    float       sphericalPolygonalLightsFirefliesClamp;
    // For which light first-person viewer shadows should be ignored.
    // E.g. first-person flashlight.
    // Null, if none.
    uint64_t    *lightUniqueIdIgnoreFirstPersonViewerShadows;
} RgDrawFrameShadowParams;

typedef struct RgDrawFrameBloomParams
{
    // Negative value disables bloom pass
    float       bloomIntensity;
    float       inputThreshold;
    float       inputThresholdLength;
    float       upsampleRadius;
    float       bloomEmissionMultiplier;
    // -1.0 is zero saturation, 0.0 is default
    float       bloomEmissionSaturationBias;
    float       bloomSkyMultiplier;
} RgDrawFrameBloomParams;

typedef struct RgPostEffectWipe
{
    // [0..1] where 1 is whole screen width.
    float       stripWidth;
    RgBool32    beginNow;
    float       duration;
} RgPostEffectWipe;

typedef struct RgPostEffectRadialBlur
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
} RgPostEffectRadialBlur;

typedef struct RgPostEffectChromaticAberration
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
    float       intensity;
} RgPostEffectChromaticAberration;

typedef struct RgPostEffectInverseBlackAndWhite
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
} RgPostEffectInverseBlackAndWhite;

typedef struct RgPostEffectHueShift
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
} RgPostEffectHueShift;

typedef struct RgPostEffectDistortedSides
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
} RgPostEffectDistortedSides;

typedef struct RgPostEffectWaves
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
    float       amplitude;
    float       speed;
    float       xMultiplier;
} RgPostEffectWaves;

typedef struct RgPostEffectColorTint
{
    RgBool32    isActive;
    float       transitionDurationIn;
    float       transitionDurationOut;
    float       intensity;
    RgFloat3D   color;
} RgPostEffectColorTint;

typedef struct RgPostEffectCRT
{
    RgBool32    isActive;
} RgPostEffectCRT;

typedef struct RgDrawFramePostEffectsParams
{
    const RgPostEffectWipe                  *pWipe;
    const RgPostEffectRadialBlur            *pRadialBlur;
    const RgPostEffectChromaticAberration   *pChromaticAberration;
    const RgPostEffectInverseBlackAndWhite  *pInverseBlackAndWhite;
    const RgPostEffectHueShift              *pHueShift;
    const RgPostEffectDistortedSides        *pDistortedSides;
    const RgPostEffectWaves                 *pWaves;
    const RgPostEffectColorTint             *pColorTint;
    const RgPostEffectCRT                   *pCRT;
} RgDrawFramePostEffectsParams;

typedef enum RgMediaType
{
    RG_MEDIA_TYPE_VACUUM,
    RG_MEDIA_TYPE_WATER,
    RG_MEDIA_TYPE_GLASS
} RgMediaType;

typedef struct RgDrawFrameReflectRefractParams
{
    RgBool32    isReflRefrAlphaTested;
    uint32_t    maxReflectRefractDepth;
    // Media type, in which camera currently is.
    RgMediaType typeOfMediaAroundCamera;
    // Should reflect-refract geometry cast shadows?
    RgBool32    reflectRefractCastShadows;
    // Should reflect-refract geometry be hit when indirect illumination is ray traced?
    // Note: indirect illumination will interpret reflect-refract geometry as non reflect-refract
    RgBool32    reflectRefractToIndirect;
    // Default: 1.52
    float       indexOfRefractionGlass;
    // Default: 1.33
    float       indexOfRefractionWater;
    RgBool32    forceNoWaterRefraction;
    float       waterWaveSpeed;
    float       waterWaveNormalStrength;
    // Default: (0.030, 0.019, 0.013)
    RgFloat3D   waterExtinction;
    // The lower this value, the sharper water normal textures.
    // Default: 1.0
    float       waterWaveTextureDerivativesMultiplier;
    // The larger this value, the larger the area one water texture covers.
    // If equals to 0.0, then default value is used.
    // Default: 1.0
    float       waterTextureAreaScale;
    // If true, reflections are disabled for backface triangles
    // of geometry that is marked RG_GEOMETRY_UPLOAD_NO_MEDIA_CHANGE_ON_REFRACT_BIT
    RgBool32    disableBackfaceReflectionsForNoMediaChange;
    // Difference between portal input and portal output world positions.
    RgFloat3D   portalInputPosition;
    RgFloat3D   portalOutputPosition;
    // Rotation of the output portal rotation relative to rotation of the input .
    RgMatrix3D  portalRelativeRotation;
} RgDrawFrameReflectRefractParams;

typedef enum RgRenderUpscaleTechnique
{
    RG_RENDER_UPSCALE_TECHNIQUE_LINEAR,
    RG_RENDER_UPSCALE_TECHNIQUE_NEAREST,
    RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
    RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS,
} RgRenderUpscaleTechnique;

typedef enum RgRenderSharpenTechnique
{
    RG_RENDER_SHARPEN_TECHNIQUE_NONE,
    RG_RENDER_SHARPEN_TECHNIQUE_NAIVE,
    RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS,
} RgRenderSharpenTechnique;

typedef enum RgRenderResolutionMode
{
    RG_RENDER_RESOLUTION_MODE_CUSTOM,
    RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE,
    RG_RENDER_RESOLUTION_MODE_PERFORMANCE,
    RG_RENDER_RESOLUTION_MODE_BALANCED,
    RG_RENDER_RESOLUTION_MODE_QUALITY,
    RG_RENDER_RESOLUTION_MODE_ULTRA_QUALITY,    // with AMD_FSR, same as QUALITY
} RgRenderResolutionMode;

typedef struct RgDrawFrameRenderResolutionParams
{
    RgRenderUpscaleTechnique    upscaleTechnique;
    RgRenderSharpenTechnique    sharpenTechnique; 
    RgRenderResolutionMode      resolutionMode;
    // Used, if resolutionMode is CUSTOM
    RgExtent2D                  renderSize;
    RgBool32                    interlacing;
} RgDrawFrameRenderResolutionParams;

typedef struct RgDrawFrameLensFlareParams
{
    RgBlendFactor               lensFlareBlendFuncSrc;
    RgBlendFactor               lensFlareBlendFuncDst;
} RgDrawFrameLensFlareParams;

typedef enum RgDrawFrameRayCullFlagBits
{
    RG_DRAW_FRAME_RAY_CULL_WORLD_0_BIT  = 1,    // RG_GEOMETRY_VISIBILITY_TYPE_WORLD_0
    RG_DRAW_FRAME_RAY_CULL_WORLD_1_BIT  = 2,    // RG_GEOMETRY_VISIBILITY_TYPE_WORLD_1
    RG_DRAW_FRAME_RAY_CULL_WORLD_2_BIT  = 4,    // RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2
    RG_DRAW_FRAME_RAY_CULL_SKY_BIT      = 8,    // RG_GEOMETRY_VISIBILITY_TYPE_SKY
} RgDrawFrameRayCullFlagBits;
typedef RgFlags RgDrawFrameRayCullFlags;

typedef struct RgDrawFrameInfo
{
    // View and projection matrices are column major.
    float                   view[16];
    float                   projection[16];
    // For additional water calculations (is the flow vertical, make extinction stronger closer to horizon).
    // If the length is close to 0.0, then (0, 1, 0) is used.
    RgFloat3D               worldUpVector;
    // Additional info for ray cones, it's used to calculate differentials for texture sampling. Also, for FSR2.
    float                   fovYRadians;
    // Camera's near and far planes are used for FSR2.
    float                   cameraNear;
    float                   cameraFar;
    // What world parts to render. First-person related geometry is always enabled.
    RgDrawFrameRayCullFlags rayCullMaskWorld;
    // Max value: 10000.0
    float                   rayLength;
    // Distance, at which primary ray starts. Can be used as a near plane distance.
    float                   primaryRayMinDist;
    RgBool32                disableRayTracing;
    RgBool32                disableRasterization;
    double                  currentTime;
    RgBool32                disableEyeAdaptation;
    RgBool32                useSqrtRoughnessForIndirect;

    // Set to null, to use default values.
    const RgDrawFrameRenderResolutionParams     *pRenderResolutionParams;
    const RgDrawFrameShadowParams               *pShadowParams;
    const RgDrawFrameTonemappingParams          *pTonemappingParams;
    const RgDrawFrameBloomParams                *pBloomParams;
    const RgDrawFrameReflectRefractParams       *pReflectRefractParams;
    const RgDrawFrameSkyParams                  *pSkyParams;
    const RgDrawFrameTexturesParams             *pTexturesParams;
    const RgDrawFrameLensFlareParams            *pLensFlareParams;
    const RgDrawFrameDebugParams                *pDebugParams;
    RgDrawFramePostEffectsParams                postEffectParams;

} RgDrawFrameInfo;

RGAPI RgResult RGCONV rgDrawFrame(
    RgInstance                          rgInstance,
    const RgDrawFrameInfo               *pDrawInfo);



RGAPI RgBool32 RGCONV rgIsRenderUpscaleTechniqueAvailable(
    RgInstance                          rgInstance,
    RgRenderUpscaleTechnique            technique);

RGAPI const char* RGCONV rgGetResultDescription(RgResult result);

#ifdef __cplusplus
}
#endif

#endif // RTGL1_H_
