// This file was generated by GenerateShaderCommon.py

#define MAX_STATIC_VERTEX_COUNT (1048576)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define MAX_INDEXED_PRIMITIVE_COUNT (1048576)
#define MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT (4096)
#define MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW (12)
#define MAX_GEOMETRY_PRIMITIVE_COUNT (1048576)
#define MAX_GEOMETRY_PRIMITIVE_COUNT_POW (20)
#define LOWER_BOTTOM_LEVEL_GEOMETRIES_COUNT (256)
#define MAX_TOP_LEVEL_INSTANCE_COUNT (45)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_INDEX_BUFFER_STATIC (2)
#define BINDING_INDEX_BUFFER_DYNAMIC (3)
#define BINDING_GEOMETRY_INSTANCES (4)
#define BINDING_GEOMETRY_INSTANCES_MATCH_PREV (5)
#define BINDING_PREV_POSITIONS_BUFFER_DYNAMIC (6)
#define BINDING_PREV_INDEX_BUFFER_DYNAMIC (7)
#define BINDING_STATIC_TEXCOORD_LAYER_1 (8)
#define BINDING_STATIC_TEXCOORD_LAYER_2 (9)
#define BINDING_STATIC_TEXCOORD_LAYER_3 (10)
#define BINDING_DYNAMIC_TEXCOORD_LAYER_1 (11)
#define BINDING_DYNAMIC_TEXCOORD_LAYER_2 (12)
#define BINDING_DYNAMIC_TEXCOORD_LAYER_3 (13)
#define BINDING_GLOBAL_UNIFORM (0)
#define BINDING_ACCELERATION_STRUCTURE_MAIN (0)
#define BINDING_TEXTURES (0)
#define BINDING_CUBEMAPS (0)
#define BINDING_RENDER_CUBEMAP (0)
#define BINDING_BLUE_NOISE (0)
#define BINDING_LUM_HISTOGRAM (0)
#define BINDING_LIGHT_SOURCES (0)
#define BINDING_LIGHT_SOURCES_PREV (1)
#define BINDING_LIGHT_SOURCES_INDEX_PREV_TO_CUR (2)
#define BINDING_LIGHT_SOURCES_INDEX_CUR_TO_PREV (3)
#define BINDING_INITIAL_LIGHTS_GRID (4)
#define BINDING_INITIAL_LIGHTS_GRID_PREV (5)
#define BINDING_LENS_FLARES_CULLING_INPUT (0)
#define BINDING_LENS_FLARES_DRAW_CMDS (1)
#define BINDING_DRAW_LENS_FLARES_INSTANCES (0)
#define BINDING_DECAL_INSTANCES (0)
#define BINDING_PORTAL_INSTANCES (0)
#define BINDING_LPM_PARAMS (0)
#define BINDING_RESTIR_INDIRECT_INITIAL_SAMPLES (0)
#define BINDING_RESTIR_INDIRECT_RESERVOIRS (1)
#define BINDING_RESTIR_INDIRECT_RESERVOIRS_PREV (2)
#define BINDING_VOLUMETRIC_STORAGE (0)
#define BINDING_VOLUMETRIC_SAMPLER (1)
#define BINDING_VOLUMETRIC_SAMPLER_PREV (2)
#define BINDING_VOLUMETRIC_ILLUMINATION (3)
#define BINDING_VOLUMETRIC_ILLUMINATION_SAMPLER (4)
#define INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC (1 << 0)
#define INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON (1 << 1)
#define INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER (1 << 2)
#define INSTANCE_CUSTOM_INDEX_FLAG_SKY (1 << 3)
#define INSTANCE_MASK_WORLD_0 (1 << 0)
#define INSTANCE_MASK_WORLD_1 (1 << 1)
#define INSTANCE_MASK_WORLD_2 (1 << 2)
#define INSTANCE_MASK_RESERVED_0 (1 << 3)
#define INSTANCE_MASK_RESERVED_1 (1 << 4)
#define INSTANCE_MASK_REFRACT (1 << 5)
#define INSTANCE_MASK_FIRST_PERSON (1 << 6)
#define INSTANCE_MASK_FIRST_PERSON_VIEWER (1 << 7)
#define PAYLOAD_INDEX_DEFAULT (0)
#define PAYLOAD_INDEX_SHADOW (1)
#define SBT_INDEX_RAYGEN_PRIMARY (0)
#define SBT_INDEX_RAYGEN_REFL_REFR (1)
#define SBT_INDEX_RAYGEN_DIRECT (2)
#define SBT_INDEX_RAYGEN_INDIRECT_INIT (3)
#define SBT_INDEX_RAYGEN_INDIRECT_FINAL (4)
#define SBT_INDEX_RAYGEN_GRADIENTS (5)
#define SBT_INDEX_RAYGEN_INITIAL_RESERVOIRS (6)
#define SBT_INDEX_RAYGEN_VOLUMETRIC (7)
#define SBT_INDEX_MISS_DEFAULT (0)
#define SBT_INDEX_MISS_SHADOW (1)
#define SBT_INDEX_HITGROUP_FULLY_OPAQUE (0)
#define SBT_INDEX_HITGROUP_ALPHA_TESTED (1)
#define MATERIAL_NO_TEXTURE (0)
#define MATERIAL_BLENDING_TYPE_OPAQUE (0)
#define MATERIAL_BLENDING_TYPE_ALPHA (1)
#define MATERIAL_BLENDING_TYPE_ADD (2)
#define MATERIAL_BLENDING_TYPE_SHADE (3)
#define MATERIAL_BLENDING_TYPE_BIT_COUNT (2)
#define MATERIAL_BLENDING_TYPE_BIT_MASK (3)
#define GEOM_INST_FLAG_BLENDING_LAYER_COUNT (4)
#define GEOM_INST_FLAG_RESERVED_0 (1 << 8)
#define GEOM_INST_FLAG_RESERVED_1 (1 << 9)
#define GEOM_INST_FLAG_RESERVED_2 (1 << 10)
#define GEOM_INST_FLAG_RESERVED_3 (1 << 11)
#define GEOM_INST_FLAG_RESERVED_4 (1 << 12)
#define GEOM_INST_FLAG_GLASS_IF_SMOOTH (1 << 13)
#define GEOM_INST_FLAG_MIRROR_IF_SMOOTH (1 << 14)
#define GEOM_INST_FLAG_EXISTS_LAYER1 (1 << 15)
#define GEOM_INST_FLAG_EXISTS_LAYER2 (1 << 16)
#define GEOM_INST_FLAG_EXISTS_LAYER3 (1 << 17)
#define GEOM_INST_FLAG_MEDIA_TYPE_ACID (1 << 18)
#define GEOM_INST_FLAG_EXACT_NORMALS (1 << 19)
#define GEOM_INST_FLAG_IGNORE_REFRACT_AFTER (1 << 20)
#define GEOM_INST_FLAG_RESERVED_5 (1 << 21)
#define GEOM_INST_FLAG_RESERVED_6 (1 << 22)
#define GEOM_INST_FLAG_THIN_MEDIA (1 << 23)
#define GEOM_INST_FLAG_REFRACT (1 << 24)
#define GEOM_INST_FLAG_REFLECT (1 << 25)
#define GEOM_INST_FLAG_PORTAL (1 << 26)
#define GEOM_INST_FLAG_MEDIA_TYPE_WATER (1 << 27)
#define GEOM_INST_FLAG_MEDIA_TYPE_GLASS (1 << 28)
#define GEOM_INST_FLAG_GENERATE_NORMALS (1 << 29)
#define GEOM_INST_FLAG_INVERTED_NORMALS (1 << 30)
#define GEOM_INST_FLAG_IS_MOVABLE (1 << 31)
#define SKY_TYPE_COLOR (0)
#define SKY_TYPE_CUBEMAP (1)
#define SKY_TYPE_RASTERIZED_GEOMETRY (2)
#define BLUE_NOISE_TEXTURE_COUNT (128)
#define BLUE_NOISE_TEXTURE_SIZE (128)
#define BLUE_NOISE_TEXTURE_SIZE_POW (7)
#define COMPUTE_COMPOSE_GROUP_SIZE_X (16)
#define COMPUTE_COMPOSE_GROUP_SIZE_Y (16)
#define COMPUTE_DECAL_APPLY_GROUP_SIZE_X (16)
#define COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_X (16)
#define COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_Y (16)
#define COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_X (16)
#define COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_Y (16)
#define COMPUTE_BLOOM_APPLY_GROUP_SIZE_X (16)
#define COMPUTE_BLOOM_APPLY_GROUP_SIZE_Y (16)
#define COMPUTE_BLOOM_STEP_COUNT (8)
#define COMPUTE_EFFECT_GROUP_SIZE_X (16)
#define COMPUTE_EFFECT_GROUP_SIZE_Y (16)
#define COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_X (16)
#define COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_Y (16)
#define COMPUTE_LUM_HISTOGRAM_BIN_COUNT (256)
#define COMPUTE_VERT_PREPROC_GROUP_SIZE_X (256)
#define VERT_PREPROC_MODE_ONLY_DYNAMIC (0)
#define VERT_PREPROC_MODE_DYNAMIC_AND_MOVABLE (1)
#define VERT_PREPROC_MODE_ALL (2)
#define GRADIENT_ESTIMATION_ENABLED (1)
#define COMPUTE_GRADIENT_ATROUS_GROUP_SIZE_X (16)
#define COMPUTE_ANTIFIREFLY_GROUP_SIZE_X (16)
#define COMPUTE_SVGF_TEMPORAL_GROUP_SIZE_X (16)
#define COMPUTE_SVGF_VARIANCE_GROUP_SIZE_X (16)
#define COMPUTE_SVGF_ATROUS_GROUP_SIZE_X (16)
#define COMPUTE_SVGF_ATROUS_ITERATION_COUNT (4)
#define COMPUTE_ASVGF_STRATA_SIZE (3)
#define COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT (4)
#define COMPUTE_INDIRECT_DRAW_FLARES_GROUP_SIZE_X (256)
#define LENS_FLARES_MAX_DRAW_CMD_COUNT (512)
#define DEBUG_SHOW_FLAG_MOTION_VECTORS (1 << 0)
#define DEBUG_SHOW_FLAG_GRADIENTS (1 << 1)
#define DEBUG_SHOW_FLAG_UNFILTERED_DIFFUSE (1 << 2)
#define DEBUG_SHOW_FLAG_UNFILTERED_SPECULAR (1 << 3)
#define DEBUG_SHOW_FLAG_UNFILTERED_INDIRECT (1 << 4)
#define DEBUG_SHOW_FLAG_ONLY_DIRECT_DIFFUSE (1 << 5)
#define DEBUG_SHOW_FLAG_ONLY_SPECULAR (1 << 6)
#define DEBUG_SHOW_FLAG_ONLY_INDIRECT_DIFFUSE (1 << 7)
#define DEBUG_SHOW_FLAG_LIGHT_GRID (1 << 8)
#define DEBUG_SHOW_FLAG_ALBEDO_WHITE (1 << 9)
#define DEBUG_SHOW_FLAG_NORMALS (1 << 10)
#define MAX_RAY_LENGTH (10000.0)
#define MEDIA_TYPE_VACUUM (0)
#define MEDIA_TYPE_WATER (1)
#define MEDIA_TYPE_GLASS (2)
#define MEDIA_TYPE_ACID (3)
#define MEDIA_TYPE_COUNT (4)
#define GEOM_INST_NO_TRIANGLE_INFO (UINT32_MAX)
#define LIGHT_TYPE_NONE (0)
#define LIGHT_TYPE_DIRECTIONAL (1)
#define LIGHT_TYPE_SPHERE (2)
#define LIGHT_TYPE_TRIANGLE (3)
#define LIGHT_TYPE_SPOT (4)
#define LIGHT_ARRAY_DIRECTIONAL_LIGHT_OFFSET (0)
#define LIGHT_ARRAY_REGULAR_LIGHTS_OFFSET (1)
#define LIGHT_INDEX_NONE (32767)
#define LIGHT_GRID_SIZE_X (16)
#define LIGHT_GRID_SIZE_Y (16)
#define LIGHT_GRID_SIZE_Z (16)
#define LIGHT_GRID_CELL_SIZE (128)
#define COMPUTE_LIGHT_GRID_GROUP_SIZE_X (256)
#define PORTAL_INDEX_NONE (63)
#define PORTAL_MAX_COUNT (63)
#define PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS (6)
#define PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS (8)
#define VOLUMETRIC_SIZE_X (160)
#define VOLUMETRIC_SIZE_Y (88)
#define VOLUMETRIC_SIZE_Z (64)
#define COMPUTE_VOLUMETRIC_GROUP_SIZE_X (16)
#define COMPUTE_VOLUMETRIC_GROUP_SIZE_Y (16)
#define COMPUTE_SCATTER_ACCUM_GROUP_SIZE_X (16)
#define VOLUME_ENABLE_NONE (0)
#define VOLUME_ENABLE_SIMPLE (1)
#define VOLUME_ENABLE_VOLUMETRIC (2)

#define SURFACE_POSITION_INCORRECT (10000000.0)

struct ShVertex
{
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec2 texCoord;
    uint color;
    uint _padding;
};

struct ShGlobalUniform
{
    mat4 view;
    mat4 invView;
    mat4 viewPrev;
    mat4 projection;
    mat4 invProjection;
    mat4 projectionPrev;
    mat4 volumeViewProj;
    mat4 volumeViewProjInv;
    mat4 volumeViewProj_Prev;
    mat4 volumeViewProjInv_Prev;
    float cellWorldSize;
    float lightmapScreenCoverage;
    uint illumVolumeEnable;
    float renderWidth;
    float renderHeight;
    uint frameId;
    float timeDelta;
    float minLogLuminance;
    float maxLogLuminance;
    float luminanceWhitePoint;
    uint stopEyeAdaptation;
    uint directionalLightExists;
    float polyLightSpotlightFactor;
    uint skyType;
    float skyColorMultiplier;
    uint skyCubemapIndex;
    vec4 skyColorDefault;
    vec4 cameraPosition;
    vec4 cameraPositionPrev;
    uint debugShowFlags;
    uint indirSecondBounce;
    uint lightCount;
    uint lightCountPrev;
    float emissionMapBoost;
    float emissionMaxScreenColor;
    float normalMapStrength;
    float skyColorSaturation;
    uint maxBounceShadowsLights;
    float rayLength;
    uint rayCullBackFaces;
    uint rayCullMaskWorld;
    float bloomIntensity;
    float bloomThreshold;
    float bloomEmissionMultiplier;
    uint reflectRefractMaxDepth;
    uint cameraMediaType;
    float indexOfRefractionWater;
    float indexOfRefractionGlass;
    float waterTextureDerivativesMultiplier;
    uint volumeEnableType;
    float volumeScattering;
    float lensDirtIntensity;
    uint waterNormalTextureIndex;
    float thinMediaWidth;
    float time;
    float waterWaveSpeed;
    float waterWaveStrength;
    vec4 waterColorAndDensity;
    vec4 acidColorAndDensity;
    float cameraRayConeSpreadAngle;
    float waterTextureAreaScale;
    uint dirtMaskTextureIndex;
    float upscaledRenderWidth;
    vec4 worldUpVector;
    float upscaledRenderHeight;
    float jitterX;
    float jitterY;
    float primaryRayMinDist;
    uint rayCullMaskWorld_Shadow;
    uint volumeAllowTintUnderwater;
    uint _unused2;
    uint twirlPortalNormal;
    uint lightIndexIgnoreFPVShadows;
    float gradientMultDiffuse;
    float gradientMultIndirect;
    float gradientMultSpecular;
    float minRoughness;
    float volumeCameraNear;
    float volumeCameraFar;
    uint antiFireflyEnabled;
    vec4 volumeAmbient;
    vec4 volumeUnderwaterColor;
    vec4 volumeFallbackSrcColor;
    vec4 volumeFallbackSrcDirection;
    float volumeAsymmetry;
    uint volumeLightSourceIndex;
    float volumeFallbackSrcExists;
    float volumeLightMult;
    ivec4 instanceGeomInfoOffset[12];
    ivec4 instanceGeomInfoOffsetPrev[12];
    ivec4 instanceGeomCount[12];
    mat4 viewProjCubemap[6];
    mat4 skyCubemapRotationTransform;
};

struct ShGeometryInstance
{
    mat4 model;
    mat4 prevModel;
    uint flags;
    uint texture_base;
    uint texture_base_ORM;
    uint texture_base_N;
    uint texture_base_E;
    uint texture_layer1;
    uint texture_layer2;
    uint texture_layer3;
    uint colorFactor_base;
    uint colorFactor_layer1;
    uint colorFactor_layer2;
    uint colorFactor_layer3;
    uint baseVertexIndex;
    uint baseIndexIndex;
    uint prevBaseVertexIndex;
    uint prevBaseIndexIndex;
    uint vertexCount;
    uint indexCount;
    float roughnessDefault;
    float metallicDefault;
    float emissiveMult;
    uint firstVertex_Layer1;
    uint firstVertex_Layer2;
    uint firstVertex_Layer3;
    uint _unused3;
    uint _unused4;
    uint _unused5;
    uint _unused6;
    uint _unused7;
    uint _unused8;
    uint _unused9;
    uint _unused10;
};

struct ShTonemapping
{
    uint histogram[256];
    float avgLuminance;
};

struct ShLightEncoded
{
    vec3 color;
    uint lightType;
    vec4 data_0;
    vec4 data_1;
    vec4 data_2;
};

struct ShLightInCell
{
    uint selected_lightIndex;
    float selected_targetPdf;
    float weightSum;
    uint __pad0;
};

struct ShVertPreprocessing
{
    uint tlasInstanceCount;
    uint tlasInstanceIsDynamicBits[2];
};

struct ShIndirectDrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
    float positionToCheck_X;
    float positionToCheck_Y;
    float positionToCheck_Z;
};

struct ShLensFlareInstance
{
    uint packedColor;
    uint textureIndex;
    uint emissiveTextureIndex;
    float emissiveMult;
};

struct ShDecalInstance
{
    mat4 transform;
    uint textureAlbedoAlpha;
    uint textureOcclusionRoughnessMetallic;
    uint textureNormal;
    uint textureEmissive;
};

struct ShPortalInstance
{
    vec4 inPosition;
    vec4 outPosition;
    vec4 outDirection;
    vec4 outUp;
};

#ifdef DESC_SET_FRAMEBUFFERS

// framebuffer indices
#define FB_IMAGE_INDEX_ALBEDO 0
#define FB_IMAGE_INDEX_IS_SKY 1
#define FB_IMAGE_INDEX_NORMAL 2
#define FB_IMAGE_INDEX_NORMAL_PREV 3
#define FB_IMAGE_INDEX_METALLIC_ROUGHNESS 4
#define FB_IMAGE_INDEX_METALLIC_ROUGHNESS_PREV 5
#define FB_IMAGE_INDEX_DEPTH_WORLD 6
#define FB_IMAGE_INDEX_DEPTH_WORLD_PREV 7
#define FB_IMAGE_INDEX_DEPTH_GRAD 8
#define FB_IMAGE_INDEX_DEPTH_NDC 9
#define FB_IMAGE_INDEX_MOTION 10
#define FB_IMAGE_INDEX_UNFILTERED_DIRECT 11
#define FB_IMAGE_INDEX_UNFILTERED_SPECULAR 12
#define FB_IMAGE_INDEX_UNFILTERED_INDIR 13
#define FB_IMAGE_INDEX_SURFACE_POSITION 14
#define FB_IMAGE_INDEX_SURFACE_POSITION_PREV 15
#define FB_IMAGE_INDEX_VISIBILITY_BUFFER 16
#define FB_IMAGE_INDEX_VISIBILITY_BUFFER_PREV 17
#define FB_IMAGE_INDEX_VIEW_DIRECTION 18
#define FB_IMAGE_INDEX_PRIMARY_TO_REFL_REFR 19
#define FB_IMAGE_INDEX_THROUGHPUT 20
#define FB_IMAGE_INDEX_PRE_FINAL 21
#define FB_IMAGE_INDEX_FINAL 22
#define FB_IMAGE_INDEX_UPSCALED_PING 23
#define FB_IMAGE_INDEX_UPSCALED_PONG 24
#define FB_IMAGE_INDEX_MOTION_DLSS 25
#define FB_IMAGE_INDEX_REACTIVITY 26
#define FB_IMAGE_INDEX_ACCUM_HISTORY_LENGTH 27
#define FB_IMAGE_INDEX_ACCUM_HISTORY_LENGTH_PREV 28
#define FB_IMAGE_INDEX_DIFF_TEMPORARY 29
#define FB_IMAGE_INDEX_DIFF_ACCUM_COLOR 30
#define FB_IMAGE_INDEX_DIFF_ACCUM_COLOR_PREV 31
#define FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS 32
#define FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS_PREV 33
#define FB_IMAGE_INDEX_DIFF_COLOR_HISTORY 34
#define FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE 35
#define FB_IMAGE_INDEX_DIFF_PONG_COLOR_AND_VARIANCE 36
#define FB_IMAGE_INDEX_SPEC_ACCUM_COLOR 37
#define FB_IMAGE_INDEX_SPEC_ACCUM_COLOR_PREV 38
#define FB_IMAGE_INDEX_SPEC_PING_COLOR 39
#define FB_IMAGE_INDEX_SPEC_PONG_COLOR 40
#define FB_IMAGE_INDEX_INDIR_ACCUM 41
#define FB_IMAGE_INDEX_INDIR_ACCUM_PREV 42
#define FB_IMAGE_INDEX_INDIR_PING 43
#define FB_IMAGE_INDEX_INDIR_PONG 44
#define FB_IMAGE_INDEX_ATROUS_FILTERED_VARIANCE 45
#define FB_IMAGE_INDEX_HISTOGRAM_INPUT 46
#define FB_IMAGE_INDEX_NORMAL_DECAL 47
#define FB_IMAGE_INDEX_SCATTERING 48
#define FB_IMAGE_INDEX_SCATTERING_PREV 49
#define FB_IMAGE_INDEX_SCATTERING_HISTORY 50
#define FB_IMAGE_INDEX_SCATTERING_HISTORY_PREV 51
#define FB_IMAGE_INDEX_ACID_FOG_R_T 52
#define FB_IMAGE_INDEX_ACID_FOG 53
#define FB_IMAGE_INDEX_SCREEN_EMIS_R_T 54
#define FB_IMAGE_INDEX_SCREEN_EMISSION 55
#define FB_IMAGE_INDEX_BLOOM_INPUT 56
#define FB_IMAGE_INDEX_BLOOM_MIP1 57
#define FB_IMAGE_INDEX_BLOOM_MIP2 58
#define FB_IMAGE_INDEX_BLOOM_MIP3 59
#define FB_IMAGE_INDEX_BLOOM_MIP4 60
#define FB_IMAGE_INDEX_BLOOM_MIP5 61
#define FB_IMAGE_INDEX_BLOOM_MIP6 62
#define FB_IMAGE_INDEX_BLOOM_MIP7 63
#define FB_IMAGE_INDEX_BLOOM_MIP8 64
#define FB_IMAGE_INDEX_BLOOM_RESULT 65
#define FB_IMAGE_INDEX_WIPE_EFFECT_SOURCE 66
#define FB_IMAGE_INDEX_RESERVOIRS 67
#define FB_IMAGE_INDEX_RESERVOIRS_PREV 68
#define FB_IMAGE_INDEX_RESERVOIRS_INITIAL 69
#define FB_IMAGE_INDEX_GRADIENT_INPUTS 70
#define FB_IMAGE_INDEX_GRADIENT_INPUTS_PREV 71
#define FB_IMAGE_INDEX_D_I_S_PING_GRADIENT 72
#define FB_IMAGE_INDEX_D_I_S_PONG_GRADIENT 73
#define FB_IMAGE_INDEX_D_I_S_GRADIENT_HISTORY 74
#define FB_IMAGE_INDEX_GRADIENT_PREV_PIX 75

// framebuffers
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 0, r11f_g11f_b10f) uniform image2D framebufAlbedo;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 1, r8ui) uniform uimage2D framebufIsSky;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 2, r32ui) uniform uimage2D framebufNormal;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 3, r32ui) uniform uimage2D framebufNormal_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 4, rg8) uniform image2D framebufMetallicRoughness;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 5, rg8) uniform image2D framebufMetallicRoughness_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 6, r16f) uniform image2D framebufDepthWorld;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 7, r16f) uniform image2D framebufDepthWorld_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 8, r16f) uniform image2D framebufDepthGrad;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 9, r32f) uniform image2D framebufDepthNdc;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 10, rgba16f) uniform image2D framebufMotion;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 11, r32ui) uniform uimage2D framebufUnfilteredDirect;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 12, r32ui) uniform uimage2D framebufUnfilteredSpecular;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 13, r32ui) uniform uimage2D framebufUnfilteredIndir;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 14, rgba32f) uniform image2D framebufSurfacePosition;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 15, rgba32f) uniform image2D framebufSurfacePosition_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 16, rgba32f) uniform image2D framebufVisibilityBuffer;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 17, rgba32f) uniform image2D framebufVisibilityBuffer_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 18, rgba16f) uniform image2D framebufViewDirection;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 19, rgba32ui) uniform uimage2D framebufPrimaryToReflRefr;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 20, rgba16f) uniform image2D framebufThroughput;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 21, r11f_g11f_b10f) uniform image2D framebufPreFinal;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 22, r11f_g11f_b10f) uniform image2D framebufFinal;
#endif
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 23, r11f_g11f_b10f) uniform image2D framebufUpscaledPing;
#endif
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 24, r11f_g11f_b10f) uniform image2D framebufUpscaledPong;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 25, rg16f) uniform image2D framebufMotionDlss;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 26, r8) uniform image2D framebufReactivity;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 27, rgba16f) uniform image2D framebufAccumHistoryLength;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 28, rgba16f) uniform image2D framebufAccumHistoryLength_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 29, r32ui) uniform uimage2D framebufDiffTemporary;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 30, r32ui) uniform uimage2D framebufDiffAccumColor;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 31, r32ui) uniform uimage2D framebufDiffAccumColor_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 32, rg16f) uniform image2D framebufDiffAccumMoments;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 33, rg16f) uniform image2D framebufDiffAccumMoments_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 34, rgba16f) uniform image2D framebufDiffColorHistory;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 35, rgba16f) uniform image2D framebufDiffPingColorAndVariance;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 36, rgba16f) uniform image2D framebufDiffPongColorAndVariance;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 37, r32ui) uniform uimage2D framebufSpecAccumColor;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 38, r32ui) uniform uimage2D framebufSpecAccumColor_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 39, r32ui) uniform uimage2D framebufSpecPingColor;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 40, r32ui) uniform uimage2D framebufSpecPongColor;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 41, r32ui) uniform uimage2D framebufIndirAccum;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 42, r32ui) uniform uimage2D framebufIndirAccum_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 43, r32ui) uniform uimage2D framebufIndirPing;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 44, r32ui) uniform uimage2D framebufIndirPong;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 45, r16f) uniform image2D framebufAtrousFilteredVariance;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 46, r16f) uniform image2D framebufHistogramInput;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 47, r32ui) uniform uimage2D framebufNormalDecal;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 48, rgba16f) uniform image2D framebufScattering;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 49, rgba16f) uniform image2D framebufScattering_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 50, r16f) uniform image2D framebufScatteringHistory;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 51, r16f) uniform image2D framebufScatteringHistory_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 52, r11f_g11f_b10f) uniform image2D framebufAcidFogRT;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 53, r11f_g11f_b10f) uniform image2D framebufAcidFog;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 54, r11f_g11f_b10f) uniform image2D framebufScreenEmisRT;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 55, r11f_g11f_b10f) uniform image2D framebufScreenEmission;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 56, r11f_g11f_b10f) uniform image2D framebufBloomInput;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 57, r11f_g11f_b10f) uniform image2D framebufBloom_Mip1;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 58, r11f_g11f_b10f) uniform image2D framebufBloom_Mip2;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 59, r11f_g11f_b10f) uniform image2D framebufBloom_Mip3;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 60, r11f_g11f_b10f) uniform image2D framebufBloom_Mip4;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 61, r11f_g11f_b10f) uniform image2D framebufBloom_Mip5;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 62, r11f_g11f_b10f) uniform image2D framebufBloom_Mip6;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 63, r11f_g11f_b10f) uniform image2D framebufBloom_Mip7;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 64, r11f_g11f_b10f) uniform image2D framebufBloom_Mip8;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 65, r11f_g11f_b10f) uniform image2D framebufBloom_Result;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 66, r11f_g11f_b10f) uniform image2D framebufWipeEffectSource;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 67, rgba32ui) uniform uimage2D framebufReservoirs;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 68, rgba32ui) uniform uimage2D framebufReservoirs_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 69, rgba32ui) uniform uimage2D framebufReservoirsInitial;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 70, rg16f) uniform image2D framebufGradientInputs;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 71, rg16f) uniform image2D framebufGradientInputs_Prev;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 72, rgba8) uniform image2D framebufDISPingGradient;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 73, rgba8) uniform image2D framebufDISPongGradient;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 74, rgba8) uniform image2D framebufDISGradientHistory;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 75, r8ui) uniform uimage2D framebufGradientPrevPix;

// samplers
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 76) uniform sampler2D framebufAlbedo_Sampler;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 77) uniform usampler2D framebufIsSky_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 78) uniform usampler2D framebufNormal_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 79) uniform usampler2D framebufNormal_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 80) uniform sampler2D framebufMetallicRoughness_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 81) uniform sampler2D framebufMetallicRoughness_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 82) uniform sampler2D framebufDepthWorld_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 83) uniform sampler2D framebufDepthWorld_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 84) uniform sampler2D framebufDepthGrad_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 85) uniform sampler2D framebufDepthNdc_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 86) uniform sampler2D framebufMotion_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 87) uniform usampler2D framebufUnfilteredDirect_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 88) uniform usampler2D framebufUnfilteredSpecular_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 89) uniform usampler2D framebufUnfilteredIndir_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 90) uniform sampler2D framebufSurfacePosition_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 91) uniform sampler2D framebufSurfacePosition_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 92) uniform sampler2D framebufVisibilityBuffer_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 93) uniform sampler2D framebufVisibilityBuffer_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 94) uniform sampler2D framebufViewDirection_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 95) uniform usampler2D framebufPrimaryToReflRefr_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 96) uniform sampler2D framebufThroughput_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 97) uniform sampler2D framebufPreFinal_Sampler;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 98) uniform sampler2D framebufFinal_Sampler;
#endif
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 99) uniform sampler2D framebufUpscaledPing_Sampler;
#endif
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 100) uniform sampler2D framebufUpscaledPong_Sampler;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 101) uniform sampler2D framebufMotionDlss_Sampler;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 102) uniform sampler2D framebufReactivity_Sampler;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 103) uniform sampler2D framebufAccumHistoryLength_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 104) uniform sampler2D framebufAccumHistoryLength_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 105) uniform usampler2D framebufDiffTemporary_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 106) uniform usampler2D framebufDiffAccumColor_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 107) uniform usampler2D framebufDiffAccumColor_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 108) uniform sampler2D framebufDiffAccumMoments_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 109) uniform sampler2D framebufDiffAccumMoments_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 110) uniform sampler2D framebufDiffColorHistory_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 111) uniform sampler2D framebufDiffPingColorAndVariance_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 112) uniform sampler2D framebufDiffPongColorAndVariance_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 113) uniform usampler2D framebufSpecAccumColor_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 114) uniform usampler2D framebufSpecAccumColor_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 115) uniform usampler2D framebufSpecPingColor_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 116) uniform usampler2D framebufSpecPongColor_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 117) uniform usampler2D framebufIndirAccum_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 118) uniform usampler2D framebufIndirAccum_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 119) uniform usampler2D framebufIndirPing_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 120) uniform usampler2D framebufIndirPong_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 121) uniform sampler2D framebufAtrousFilteredVariance_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 122) uniform sampler2D framebufHistogramInput_Sampler;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 123) uniform usampler2D framebufNormalDecal_Sampler;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 124) uniform sampler2D framebufScattering_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 125) uniform sampler2D framebufScattering_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 126) uniform sampler2D framebufScatteringHistory_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 127) uniform sampler2D framebufScatteringHistory_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 128) uniform sampler2D framebufAcidFogRT_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 129) uniform sampler2D framebufAcidFog_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 130) uniform sampler2D framebufScreenEmisRT_Sampler;
#ifndef FRAMEBUF_IGNORE_ATTACHMENTS
layout(set = DESC_SET_FRAMEBUFFERS, binding = 131) uniform sampler2D framebufScreenEmission_Sampler;
#endif
layout(set = DESC_SET_FRAMEBUFFERS, binding = 132) uniform sampler2D framebufBloomInput_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 133) uniform sampler2D framebufBloom_Mip1_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 134) uniform sampler2D framebufBloom_Mip2_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 135) uniform sampler2D framebufBloom_Mip3_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 136) uniform sampler2D framebufBloom_Mip4_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 137) uniform sampler2D framebufBloom_Mip5_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 138) uniform sampler2D framebufBloom_Mip6_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 139) uniform sampler2D framebufBloom_Mip7_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 140) uniform sampler2D framebufBloom_Mip8_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 141) uniform sampler2D framebufBloom_Result_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 142) uniform sampler2D framebufWipeEffectSource_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 143) uniform usampler2D framebufReservoirs_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 144) uniform usampler2D framebufReservoirs_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 145) uniform usampler2D framebufReservoirsInitial_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 146) uniform sampler2D framebufGradientInputs_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 147) uniform sampler2D framebufGradientInputs_Prev_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 148) uniform sampler2D framebufDISPingGradient_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 149) uniform sampler2D framebufDISPongGradient_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 150) uniform sampler2D framebufDISGradientHistory_Sampler;
layout(set = DESC_SET_FRAMEBUFFERS, binding = 151) uniform usampler2D framebufGradientPrevPix_Sampler;

// pack/unpack formats
void imageStoreUnfilteredDirect(const ivec2 pix, const vec3 unpacked) { imageStore(framebufUnfilteredDirect, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchUnfilteredDirect(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufUnfilteredDirect_Sampler, pix, 0).r); }

void imageStoreUnfilteredSpecular(const ivec2 pix, const vec3 unpacked) { imageStore(framebufUnfilteredSpecular, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchUnfilteredSpecular(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufUnfilteredSpecular_Sampler, pix, 0).r); }

void imageStoreUnfilteredIndir(const ivec2 pix, const vec3 unpacked) { imageStore(framebufUnfilteredIndir, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchUnfilteredIndir(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufUnfilteredIndir_Sampler, pix, 0).r); }

void imageStoreDiffTemporary(const ivec2 pix, const vec3 unpacked) { imageStore(framebufDiffTemporary, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchDiffTemporary(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufDiffTemporary_Sampler, pix, 0).r); }

void imageStoreDiffAccumColor(const ivec2 pix, const vec3 unpacked) { imageStore(framebufDiffAccumColor, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchDiffAccumColor(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufDiffAccumColor_Sampler, pix, 0).r); }
vec3 texelFetchDiffAccumColor_Prev(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufDiffAccumColor_Prev_Sampler, pix, 0).r); }

void imageStoreSpecAccumColor(const ivec2 pix, const vec3 unpacked) { imageStore(framebufSpecAccumColor, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchSpecAccumColor(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufSpecAccumColor_Sampler, pix, 0).r); }
vec3 texelFetchSpecAccumColor_Prev(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufSpecAccumColor_Prev_Sampler, pix, 0).r); }

void imageStoreSpecPingColor(const ivec2 pix, const vec3 unpacked) { imageStore(framebufSpecPingColor, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchSpecPingColor(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufSpecPingColor_Sampler, pix, 0).r); }

void imageStoreSpecPongColor(const ivec2 pix, const vec3 unpacked) { imageStore(framebufSpecPongColor, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchSpecPongColor(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufSpecPongColor_Sampler, pix, 0).r); }

void imageStoreIndirAccum(const ivec2 pix, const vec3 unpacked) { imageStore(framebufIndirAccum, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchIndirAccum(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufIndirAccum_Sampler, pix, 0).r); }
vec3 texelFetchIndirAccum_Prev(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufIndirAccum_Prev_Sampler, pix, 0).r); }

void imageStoreIndirPing(const ivec2 pix, const vec3 unpacked) { imageStore(framebufIndirPing, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchIndirPing(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufIndirPing_Sampler, pix, 0).r); }

void imageStoreIndirPong(const ivec2 pix, const vec3 unpacked) { imageStore(framebufIndirPong, pix, uvec4(encodeE5B9G9R9(unpacked))); }
vec3 texelFetchIndirPong(const ivec2 pix){ return decodeE5B9G9R9(texelFetch(framebufIndirPong_Sampler, pix, 0).r); }


#endif
