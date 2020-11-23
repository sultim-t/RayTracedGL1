#pragma once

#ifdef __cplusplus
extern "C" {
#endif


    typedef enum RgResult
    {
        RG_SUCCESS = 0,
        RG_ERROR = 1
    } RgResult;

    //typedef enum RgStructureType
    //{
    //    RG_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    //} RgStructureType;

    typedef uint64_t RgInstance;
    typedef uint64_t RgGeometry;
    typedef uint64_t RgTexture;
    typedef uint64_t RgAnimatedTexture;
    typedef uint64_t RgDynamicTexture;

    typedef struct RgInstanceCreateInfo
    {
        //RgStructureType sType;
        const char      *name;
        // size that must be allocated for rasterized geometry, in bytes;
        // it can't be changed after rgCreateInstance; if buffer is full,
        // rasterized data will be ignored
        uint32_t        rasterizedDataBufferSize;
        // postfixes will be used to determine textures that should be 
        // loaded from files if the texture should be overridden
        // i.e. if postfix="_n" then "Floor_01" => "Floor_01_n.*", 
        // where "*" is some image extension
        char            *overrideAlbedoRoughnessTexturePostfix;
        char            *overrideNormalMetallicTexturePostfix;
        char            *overrideEmissionSpecularityTexturePostfix;
    } RgInstanceCreateInfo;

    RgResult rgCreateInstance(
        const RgInstanceCreateInfo  *info,
        RgInstance                  *result);

    typedef enum RgGeometryType
    {
        RG_GEOMETRY_TYPE_STATIC,
        RG_GEOMETRY_TYPE_STATIC_MOVABLE,
        RG_GEOMETRY_TYPE_DYNAMIC
    } RgGeometryType;

    typedef struct RgLayeredMaterial
    {
        // geometry or each triangle can have up to 3 materials, 0 is no material
        uint32_t    layerMaterialIds[3];
    } RgLayeredMaterial;

    typedef struct RgTransform
    {
        float       matrix[3][4];
    } RgTransform;

    typedef struct RgGeometryCreateInfo
    {
        RgGeometryType          geomType;

        uint32_t                vertexCount;
        // 3 floats
        float                   *vertexData;
        // each attribute has its own stride for ability to describe vertices 
        // that are packed into array of structs (i.e. Vertex[] where Vertex={Position, Normal, ...}) 
        // or separated arrays of attribute values (i.e. Positions[], Normals[], ...)
        uint32_t                vertexStride;
        // 3 floats, can be null
        float                   *normalData;
        uint32_t                normalStride;
        // 2 floats, can be null
        float                   *texCoordData;
        uint32_t                texCoordStride;
        // RGBA packed into 32-bit uint, can be null
        uint32_t                *colorData;
        uint32_t                colorStride;

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
        bool                depthTest;
        bool                depthWrite;
        bool                alphaTest;
        bool                blendEnable;
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

    typedef struct RgTextureCreateInfo
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
        bool                    enableOverride;
        float                   defaultRoughness;
        float                   defaultMetallicity;
        float                   defaultSpecularity;
        char                    *name;
        char                    *path;
    } RgTextureCreateInfo;
    
    typedef struct RgDynamicTextureInfo
    {
        RgExtent2D              size;
        uint32_t                *data;
        RgSamplerAddressMode    addressModeU;
        RgSamplerAddressMode    addressModeV;
        float                   defaultRoughness;
        float                   defaultMetallicity;
        float                   defaultSpecularity;
    } RgTextureUpdateInfo;
    
    typedef struct RgAnimatedTextureCreateInfo
    {
        uint32_t                frameCount;
        RgTextureCreateInfo     *frames;
    } RgAnimatedTextureCreateInfo;

    RgResult rgCreateTexture(
        RgInstance                          rgInstance,
        const RgTextureCreateInfo           *createInfo,
        RgTexture                           *result);

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

    RgResult rgUpdateTexture(
        RgInstance                          rgInstance,
        RgDynamicTexture                    dynamicTexture,
        const RgDynamicTextureInfo          *updateInfo);

#ifdef __cplusplus
}
#endif