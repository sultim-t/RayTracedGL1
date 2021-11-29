# Copyright (c) 2020-2021 Sultim Tsyrendashiev
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This script generates two separate header files for C and GLSL but with identical data

import sys
import os
import re
from math import log2


TYPE_FLOAT32    = 0
TYPE_INT32      = 1
TYPE_UINT32     = 2


C_TYPE_NAMES = {
    TYPE_FLOAT32:       "float",
    TYPE_INT32:         "int32_t",
    TYPE_UINT32:        "uint32_t",
}

GLSL_TYPE_NAMES = {
    TYPE_FLOAT32:       "float",
    TYPE_INT32:         "int",
    TYPE_UINT32:        "uint",
    (TYPE_FLOAT32,  2): "vec2",
    (TYPE_FLOAT32,  3): "vec3",
    (TYPE_FLOAT32,  4): "vec4",
    (TYPE_INT32,    2): "ivec2",
    (TYPE_INT32,    3): "ivec3",
    (TYPE_INT32,    4): "ivec4",
    (TYPE_UINT32,   2): "uvec2",
    (TYPE_UINT32,   3): "uvec3",
    (TYPE_UINT32,   4): "uvec4",
    (TYPE_FLOAT32, 22): "mat2",
    (TYPE_FLOAT32, 23): "mat2x3",
    (TYPE_FLOAT32, 32): "mat3x2",
    (TYPE_FLOAT32, 33): "mat3",
    (TYPE_FLOAT32, 34): "mat3x4",
    (TYPE_FLOAT32, 43): "mat4x3",
    (TYPE_FLOAT32, 44): "mat4",
}


TYPE_ACTUAL_SIZES = {
    TYPE_FLOAT32:   4,
    TYPE_INT32:     4,
    TYPE_UINT32:    4,
    (TYPE_FLOAT32,  2): 8,
    (TYPE_FLOAT32,  3): 12,
    (TYPE_FLOAT32,  4): 16,
    (TYPE_INT32,    2): 8,
    (TYPE_INT32,    3): 12,
    (TYPE_INT32,    4): 16,
    (TYPE_UINT32,   2): 8,
    (TYPE_UINT32,   3): 12,
    (TYPE_UINT32,   4): 16,
    (TYPE_FLOAT32, 22): 16,
    (TYPE_FLOAT32, 23): 24,
    (TYPE_FLOAT32, 32): 24,
    (TYPE_FLOAT32, 33): 36,
    (TYPE_FLOAT32, 34): 48,
    (TYPE_FLOAT32, 43): 48,
    (TYPE_FLOAT32, 44): 64,
}

GLSL_TYPE_SIZES_STD_430 = {
    TYPE_FLOAT32:   4,
    TYPE_INT32:     4,
    TYPE_UINT32:    4,
    (TYPE_FLOAT32,  2): 8,
    (TYPE_FLOAT32,  3): 16,
    (TYPE_FLOAT32,  4): 16,
    (TYPE_INT32,    2): 8,
    (TYPE_INT32,    3): 16,
    (TYPE_INT32,    4): 16,
    (TYPE_UINT32,   2): 8,
    (TYPE_UINT32,   3): 16,
    (TYPE_UINT32,   4): 16,
    (TYPE_FLOAT32, 22): 16,
    (TYPE_FLOAT32, 23): 24,
    (TYPE_FLOAT32, 32): 24,
    (TYPE_FLOAT32, 33): 36,
    (TYPE_FLOAT32, 34): 48,
    (TYPE_FLOAT32, 43): 48,
    (TYPE_FLOAT32, 44): 64,
}


# These types are only for image format use!
TYPE_UNORM8     = 3
TYPE_UINT16     = 4
TYPE_FLOAT16    = 5
TYPE_PACK_11    = 128   # R11G11B10
TYPE_PACK_E5    = 129   # shared exponent, E5B9G9R9

COMPONENT_R     = 0
COMPONENT_RG    = 1
COMPONENT_RGB   = 2
COMPONENT_RGBA  = 3


VULKAN_IMAGE_FORMATS = {
    (TYPE_UNORM8,   COMPONENT_R):       "VK_FORMAT_R8_UNORM",
    (TYPE_UNORM8,   COMPONENT_RG):      "VK_FORMAT_R8G8_UNORM",
    (TYPE_UNORM8,   COMPONENT_RGBA):    "VK_FORMAT_R8G8B8A8_UNORM",

    (TYPE_UINT16,   COMPONENT_R):       "VK_FORMAT_R16_UINT",
    (TYPE_UINT16,   COMPONENT_RG):      "VK_FORMAT_R16G16_UINT",
    (TYPE_UINT16,   COMPONENT_RGBA):    "VK_FORMAT_R16G16B16A16_UINT",

    (TYPE_UINT32,   COMPONENT_R):       "VK_FORMAT_R32_UINT",
    (TYPE_UINT32,   COMPONENT_RG):      "VK_FORMAT_R32G32_UINT",
    (TYPE_UINT32,   COMPONENT_RGBA):    "VK_FORMAT_R32G32B32A32_UINT",

    (TYPE_FLOAT16,  COMPONENT_R):       "VK_FORMAT_R16_SFLOAT",
    (TYPE_FLOAT16,  COMPONENT_RG):      "VK_FORMAT_R16G16_SFLOAT",
    (TYPE_FLOAT16,  COMPONENT_RGBA):    "VK_FORMAT_R16G16B16A16_SFLOAT",

    (TYPE_FLOAT32,  COMPONENT_R):       "VK_FORMAT_R32_SFLOAT",
    (TYPE_FLOAT32,  COMPONENT_RG):      "VK_FORMAT_R32G32_SFLOAT",
    (TYPE_FLOAT32,  COMPONENT_RGBA):    "VK_FORMAT_R32G32B32A32_SFLOAT",

    (TYPE_PACK_11,  COMPONENT_RGB):    "VK_FORMAT_B10G11R11_UFLOAT_PACK32",
    # Should've been VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 
    # but not enough devices support storage images with such format.
    # So pack/unpack is done manually.
    (TYPE_PACK_E5,  COMPONENT_RGB):    "VK_FORMAT_R32_UINT",
}

GLSL_IMAGE_FORMATS = {
    (TYPE_UNORM8,   COMPONENT_R):       "r8",
    (TYPE_UNORM8,   COMPONENT_RG):      "rg8",
    (TYPE_UNORM8,   COMPONENT_RGBA):    "rgba8",

    (TYPE_UINT16,   COMPONENT_R):       "r16ui",
    (TYPE_UINT16,   COMPONENT_RG):      "rg16ui",
    (TYPE_UINT16,   COMPONENT_RGBA):    "rgba16ui",

    (TYPE_UINT32,   COMPONENT_R):       "r32ui",
    (TYPE_UINT32,   COMPONENT_RG):      "rg32ui",
    (TYPE_UINT32,   COMPONENT_RGBA):    "rgba32ui",

    (TYPE_FLOAT16,  COMPONENT_R):       "r16f",
    (TYPE_FLOAT16,  COMPONENT_RG):      "rg16f",
    (TYPE_FLOAT16,  COMPONENT_RGBA):    "rgba16f",

    (TYPE_FLOAT32,  COMPONENT_R):       "r32f",
    (TYPE_FLOAT32,  COMPONENT_RG):      "rg32f",
    (TYPE_FLOAT32,  COMPONENT_RGBA):    "rgba32f",

    (TYPE_PACK_11,  COMPONENT_RGB):    "r11f_g11f_b10f",
    (TYPE_PACK_E5,  COMPONENT_RGB):    "r32ui",
}

GLSL_IMAGE_2D_TYPE = { 
    TYPE_FLOAT32    : "image2D",
    TYPE_INT32      : "iimage2D",
    TYPE_UINT32     : "uimage2D",
    TYPE_UNORM8     : "image2D",
    TYPE_UINT16     : "uimage2D",
    TYPE_FLOAT16    : "image2D",
    TYPE_PACK_11    : "image2D",
    TYPE_PACK_E5    : "uimage2D",
}

GLSL_SAMPLER_2D_TYPE = { 
    TYPE_FLOAT32    : "sampler2D",
    TYPE_INT32      : "isampler2D",
    TYPE_UINT32     : "usampler2D",
    TYPE_UNORM8     : "sampler2D",
    TYPE_UINT16     : "usampler2D",
    TYPE_FLOAT16    : "sampler2D",
    TYPE_PACK_11    : "sampler2D",
    TYPE_PACK_E5    : "usampler2D",
}


USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE = False
USE_MULTIDIMENSIONAL_ARRAYS_IN_C = False
CONST_TO_EVALUATE = "CONST VALUE MUST BE EVALUATED"







# --------------------------------------------------------------------------------------------- #
# User defined constants
# --------------------------------------------------------------------------------------------- #

CONST = {
    "MAX_STATIC_VERTEX_COUNT"               : 1 << 20,
    "MAX_DYNAMIC_VERTEX_COUNT"              : 1 << 21,
    "MAX_INDEXED_PRIMITIVE_COUNT"           : 1 << 20,
   
    "MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT"     : 1 << 12,
    "MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW" : CONST_TO_EVALUATE,
    "MAX_GEOMETRY_PRIMITIVE_COUNT"          : CONST_TO_EVALUATE,
    "MAX_GEOMETRY_PRIMITIVE_COUNT_POW"      : CONST_TO_EVALUATE,
    # used for first-person geometries
    "LOWER_BOTTOM_LEVEL_GEOMETRIES_COUNT"   : 1 << 8,
    
    "MAX_TOP_LEVEL_INSTANCE_COUNT"          : 45,
    
    "BINDING_VERTEX_BUFFER_STATIC"          : 0,
    "BINDING_VERTEX_BUFFER_DYNAMIC"         : 1,
    "BINDING_INDEX_BUFFER_STATIC"           : 2,
    "BINDING_INDEX_BUFFER_DYNAMIC"          : 3,
    "BINDING_GEOMETRY_INSTANCES"            : 4,
    "BINDING_GEOMETRY_INSTANCES_MATCH_PREV" : 5,
    "BINDING_PREV_POSITIONS_BUFFER_DYNAMIC" : 6,
    "BINDING_PREV_INDEX_BUFFER_DYNAMIC"     : 7,
    "BINDING_GLOBAL_UNIFORM"                : 0,
    "BINDING_ACCELERATION_STRUCTURE_MAIN"   : 0,
    "BINDING_TEXTURES"                      : 0,
    "BINDING_CUBEMAPS"                      : 0,
    "BINDING_RENDER_CUBEMAP"                : 0,
    "BINDING_BLUE_NOISE"                    : 0,
    "BINDING_LUM_HISTOGRAM"                 : 0,
    "BINDING_LIGHT_SOURCES_SPHERICAL"       : 0,
    "BINDING_LIGHT_SOURCES_DIRECTIONAL"     : 1,
    "BINDING_LIGHT_SOURCES_SPH_MATCH_PREV"  : 2,
    "BINDING_LIGHT_SOURCES_DIR_MATCH_PREV"  : 3,
    
    "INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC"                : "1 << 0",
    "INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON"           : "1 << 1",
    "INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER"    : "1 << 2",
    "INSTANCE_CUSTOM_INDEX_FLAG_REFLECT_REFRACT"        : "1 << 3",

    "INSTANCE_MASK_ALL"                     : "0xFF",
    "INSTANCE_MASK_WORLD_MIN"               : "0",
    "INSTANCE_MASK_WORLD_ALL"               : CONST_TO_EVALUATE,
    "INSTANCE_MASK_WORLD_0"                 : 1 << 0,
    "INSTANCE_MASK_WORLD_1"                 : 1 << 1,
    "INSTANCE_MASK_WORLD_2"                 : 1 << 2,
    "INSTANCE_MASK_RESERVED_0"              : 1 << 3,
    "INSTANCE_MASK_RESERVED_1"              : 1 << 4,
    "INSTANCE_MASK_REFLECT_REFRACT"         : 1 << 5,
    "INSTANCE_MASK_FIRST_PERSON"            : 1 << 6,
    "INSTANCE_MASK_FIRST_PERSON_VIEWER"     : 1 << 7,
    
    "PAYLOAD_INDEX_DEFAULT"                 : 0,
    "PAYLOAD_INDEX_SHADOW"                  : 1,
    
    "SBT_INDEX_RAYGEN_PRIMARY"              : 0,
    "SBT_INDEX_RAYGEN_DIRECT"               : 1,
    "SBT_INDEX_RAYGEN_INDIRECT"             : 2,
    "SBT_INDEX_MISS_DEFAULT"                : 0,
    "SBT_INDEX_MISS_SHADOW"                 : 1,
    "SBT_INDEX_HITGROUP_FULLY_OPAQUE"       : 0,
    "SBT_INDEX_HITGROUP_ALPHA_TESTED"       : 1,
    
    "MATERIAL_ALBEDO_ALPHA_INDEX"                   : 0,
    "MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX"    : 1,
    "MATERIAL_NORMAL_INDEX"                         : 2,
    
    "MATERIAL_NO_TEXTURE"                   : 0,

    "MATERIAL_BLENDING_FLAG_OPAQUE"         : "1 << 0",
    "MATERIAL_BLENDING_FLAG_ALPHA"          : "1 << 1",
    "MATERIAL_BLENDING_FLAG_ADD"            : "1 << 2",
    "MATERIAL_BLENDING_FLAG_SHADE"          : "1 << 3",
    "MATERIAL_BLENDING_FLAG_BIT_COUNT"      : 4,
    "MATERIAL_BLENDING_MASK_FIRST_LAYER"    : CONST_TO_EVALUATE,
    "MATERIAL_BLENDING_MASK_SECOND_LAYER"   : CONST_TO_EVALUATE,
    "MATERIAL_BLENDING_MASK_THIRD_LAYER"    : CONST_TO_EVALUATE,
    # 12 first bits are for the blending flags per each layer, others can be used
    "GEOM_INST_FLAG_NO_MEDIA_CHANGE"        : "1 << 23",
    "GEOM_INST_FLAG_REFRACT"                : "1 << 24",
    "GEOM_INST_FLAG_REFLECT"                : "1 << 25",
    "GEOM_INST_FLAG_PORTAL"                 : "1 << 26",
    "GEOM_INST_FLAG_MEDIA_TYPE_WATER"       : "1 << 27",
    "GEOM_INST_FLAG_MEDIA_TYPE_GLASS"       : "1 << 28",
    "GEOM_INST_FLAG_GENERATE_NORMALS"       : "1 << 29",
    "GEOM_INST_FLAG_INVERTED_NORMALS"       : "1 << 30",
    "GEOM_INST_FLAG_IS_MOVABLE"             : "1 << 31",

    "SKY_TYPE_COLOR"                        : 0,
    "SKY_TYPE_CUBEMAP"                      : 1,
    "SKY_TYPE_RASTERIZED_GEOMETRY"          : 2,
    
    "BLUE_NOISE_TEXTURE_COUNT"              : 128,
    "BLUE_NOISE_TEXTURE_SIZE"               : 128,
    "BLUE_NOISE_TEXTURE_SIZE_POW"           : CONST_TO_EVALUATE,

    "COMPUTE_COMPOSE_GROUP_SIZE_X"          : 16,
    "COMPUTE_COMPOSE_GROUP_SIZE_Y"          : 16,
    
    "COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_X"   : 16,
    "COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_Y"   : 16,
    "COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_X" : 16,
    "COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_Y" : 16,
    "COMPUTE_BLOOM_STEP_COUNT"              : 5,

    "COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_X"    : 16,
    "COMPUTE_LUM_HISTOGRAM_GROUP_SIZE_Y"    : 16,
    "COMPUTE_LUM_HISTOGRAM_BIN_COUNT"       : 256,

    "COMPUTE_VERT_PREPROC_GROUP_SIZE_X"     : 256,
    "VERT_PREPROC_MODE_ONLY_DYNAMIC"        : 0,
    "VERT_PREPROC_MODE_DYNAMIC_AND_MOVABLE" : 1,
    "VERT_PREPROC_MODE_ALL"                 : 2,

    "COMPUTE_GRADIENT_SAMPLES_GROUP_SIZE_X" : 16,
    "COMPUTE_GRADIENT_MERGING_GROUP_SIZE_X" : 16,
    "COMPUTE_GRADIENT_ATROUS_GROUP_SIZE_X"  : 16,
    "COMPUTE_SVGF_TEMPORAL_GROUP_SIZE_X"    : 16,
    "COMPUTE_SVGF_VARIANCE_GROUP_SIZE_X"    : 16,
    "COMPUTE_SVGF_ATROUS_GROUP_SIZE_X"      : 16,
    "COMPUTE_SVGF_ATROUS_ITERATION_COUNT"   : 4,

    "COMPUTE_ASVGF_STRATA_SIZE"                         : 3,
    "COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT"     : 4,  
    
    "MAX_RAY_LENGTH"                        : "10000.0",

    "MEDIA_TYPE_VACUUM"                     : 0,
    "MEDIA_TYPE_WATER"                      : 1,
    "MEDIA_TYPE_GLASS"                      : 2,
    "MEDIA_TYPE_COUNT"                      : 3,
}

CONST_GLSL_ONLY = {
    "FIDELITY_SUPER_RESOLUTION_GAMMA_SPACE" : 3.0   # FSR works in perceptual space
}


def align(c, alignment):
    return ((c + alignment - 1) // alignment) * alignment


def align4(a):
    return ((a + 3) >> 2) << 2


def evalConst():
    # flags for each layer
    assert CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"] * 3 <= 12
    CONST["MATERIAL_BLENDING_MASK_FIRST_LAYER"]     = ((1 << CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"]) - 1) << (CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"] * 0)
    CONST["MATERIAL_BLENDING_MASK_SECOND_LAYER"]    = ((1 << CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"]) - 1) << (CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"] * 1)
    CONST["MATERIAL_BLENDING_MASK_THIRD_LAYER"]     = ((1 << CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"]) - 1) << (CONST["MATERIAL_BLENDING_FLAG_BIT_COUNT"] * 2)

    CONST["MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW"]  = int(log2(CONST["MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT"]))
    CONST["MAX_GEOMETRY_PRIMITIVE_COUNT_POW"]       = 32 - CONST["MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT_POW"]
    CONST["MAX_GEOMETRY_PRIMITIVE_COUNT"]           = 1 << CONST["MAX_GEOMETRY_PRIMITIVE_COUNT_POW"]
    CONST["BLUE_NOISE_TEXTURE_SIZE_POW"]            = int(log2(CONST["BLUE_NOISE_TEXTURE_SIZE"]))

    CONST["INSTANCE_MASK_WORLD_ALL"] = CONST["INSTANCE_MASK_WORLD_0"] | CONST["INSTANCE_MASK_WORLD_1"] | CONST["INSTANCE_MASK_WORLD_2"]

    assert len([None for _, v in CONST.items() if v == CONST_TO_EVALUATE]) == 0, "All CONST_TO_EVALUATE values must be calculated"


# --------------------------------------------------------------------------------------------- #
# User defined structs
# --------------------------------------------------------------------------------------------- #
# Each member is defined as a tuple (base type, dimensions, name, count).
# Dimensions:   1 - scalar, 
#               2 - gvec2,
#               3 - gvec3, 
#               4 - gvec4, 
#               [xy] - gmat[xy] (i.e. 32 - gmat32)
# If count > 1 and dimensions is 2, 3 or 4 (matrices are not supported)
# then it'll be represented as an array with size (count*dimensions).

STATIC_BUFFER_STRUCT = [
    (TYPE_FLOAT32,      3,     "positions",             CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      3,     "normals",               CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      2,     "texCoords",             CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      2,     "texCoordsLayer1",       CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      2,     "texCoordsLayer2",       CONST["MAX_STATIC_VERTEX_COUNT"]),
]

DYNAMIC_BUFFER_STRUCT = [
    (TYPE_FLOAT32,      3,     "positions",             CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      3,     "normals",               CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_FLOAT32,      2,     "texCoords",             CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
]

# Must be careful with std140 offsets! They are set manually.
# Other structs are using std430 and padding is done automatically.
GLOBAL_UNIFORM_STRUCT = [
    (TYPE_FLOAT32,     44,      "view",                         1),
    (TYPE_FLOAT32,     44,      "invView",                      1),
    (TYPE_FLOAT32,     44,      "viewPrev",                     1),
    (TYPE_FLOAT32,     44,      "projection",                   1),
    (TYPE_FLOAT32,     44,      "invProjection",                1),
    (TYPE_FLOAT32,     44,      "projectionPrev",               1),

    (TYPE_UINT32,       1,      "positionsStride",              1),
    (TYPE_UINT32,       1,      "normalsStride",                1),
    (TYPE_UINT32,       1,      "texCoordsStride",              1),
    (TYPE_FLOAT32,      1,      "renderWidth",                  1),

    (TYPE_FLOAT32,      1,      "renderHeight",                 1),
    (TYPE_UINT32,       1,      "frameId",                      1),
    (TYPE_FLOAT32,      1,      "timeDelta",                    1),
    (TYPE_FLOAT32,      1,      "minLogLuminance",              1),

    (TYPE_FLOAT32,      1,      "maxLogLuminance",              1),
    (TYPE_FLOAT32,      1,      "luminanceWhitePoint",          1),
    (TYPE_UINT32,       1,      "stopEyeAdaptation",            1),
    (TYPE_UINT32,       1,      "lightCountSpherical",          1),
    
    (TYPE_UINT32,       1,      "lightCountDirectional",        1),
    (TYPE_UINT32,       1,      "skyType",                      1),
    (TYPE_FLOAT32,      1,      "skyColorMultiplier",           1),
    (TYPE_UINT32,       1,      "skyCubemapIndex",              1),
    
    (TYPE_FLOAT32,      4,      "skyColorDefault",              1),
    
    (TYPE_FLOAT32,      4,      "skyViewerPosition",            1),

    (TYPE_FLOAT32,      4,      "cameraPosition",               1),

    (TYPE_UINT32,       1,      "dbgShowMotionVectors",         1),
    (TYPE_UINT32,       1,      "dbgShowGradients",             1),
    (TYPE_UINT32,       1,      "lightCountSphericalPrev",      1),
    (TYPE_UINT32,       1,      "lightCountDirectionalPrev",    1),

    (TYPE_FLOAT32,      1,      "emissionMapBoost",             1),
    (TYPE_FLOAT32,      1,      "emissionMaxScreenColor",       1),
    (TYPE_FLOAT32,      1,      "normalMapStrength",            1),
    (TYPE_FLOAT32,      1,      "skyColorSaturation",           1),

    (TYPE_FLOAT32,      4,      "spotlightPosition",            1),
    (TYPE_FLOAT32,      4,      "spotlightDirection",           1),
    (TYPE_FLOAT32,      4,      "spotlightUpVector",            1),
    (TYPE_FLOAT32,      4,      "spotlightColor",               1),

    (TYPE_FLOAT32,      1,      "spotlightRadius",              1),
    (TYPE_FLOAT32,      1,      "spotlightCosAngleOuter",       1),
    (TYPE_FLOAT32,      1,      "spotlightCosAngleInner",       1),
    (TYPE_FLOAT32,      1,      "spotlightFalloffDistance",     1),

    (TYPE_UINT32,       1,      "maxBounceShadowsDirectionalLights",1),
    (TYPE_UINT32,       1,      "maxBounceShadowsSphereLights",     1),
    (TYPE_UINT32,       1,      "maxBounceShadowsSpotlights",       1),
    (TYPE_UINT32,       1,      "rayCullMaskWorld",                 1),

    (TYPE_FLOAT32,      1,      "bloomThreshold",                   1),
    (TYPE_FLOAT32,      1,      "bloomThresholdLength",             1),
    (TYPE_FLOAT32,      1,      "bloomUpsampleRadius",              1),
    (TYPE_FLOAT32,      1,      "bloomIntensity",                   1),

    (TYPE_FLOAT32,      1,      "bloomEmissionMultiplier",          1),
    (TYPE_FLOAT32,      1,      "bloomSkyMultiplier",               1),
    (TYPE_FLOAT32,      1,      "rayLength",                        1),
    (TYPE_UINT32,       1,      "reflectRefractMaxDepth",           1),

    (TYPE_UINT32,       1,      "cameraMediaType",                  1),
    (TYPE_FLOAT32,      1,      "indexOfRefractionWater",           1),
    (TYPE_FLOAT32,      1,      "indexOfRefractionGlass",           1),
    (TYPE_FLOAT32,      1,      "waterTextureDerivativesMultiplier",1),

    (TYPE_UINT32,       1,      "enableShadowsFromReflRefr",        1),
    (TYPE_UINT32,       1,      "enableIndirectFromReflRefr",       1),
    (TYPE_UINT32,       1,      "forceNoWaterRefraction",           1),
    (TYPE_UINT32,       1,      "waterNormalTextureIndex",          1),

    (TYPE_FLOAT32,      1,      "noBackfaceReflForNoMediaChange",   1),
    (TYPE_FLOAT32,      1,      "time",                             1),
    (TYPE_FLOAT32,      1,      "waterWaveSpeed",                   1),
    (TYPE_FLOAT32,      1,      "waterWaveStrength",                1),

    (TYPE_FLOAT32,      4,      "waterExtinction",                  1),

    (TYPE_FLOAT32,      4,      "portalInputToOutputTransform0",    1),

    (TYPE_FLOAT32,      4,      "portalInputToOutputTransform1",    1),

    (TYPE_FLOAT32,      4,      "portalInputToOutputTransform2",    1),

    (TYPE_FLOAT32,      4,      "portalInputPosition",              1),

    (TYPE_FLOAT32,      1,      "cameraRayConeSpreadAngle",         1),
    (TYPE_FLOAT32,      1,      "waterTextureAreaScale",            1),
    (TYPE_UINT32,       1,      "useSqrtRoughnessForIndirect",      1),
    (TYPE_FLOAT32,      1,      "_pad3",                            1),

    (TYPE_FLOAT32,      4,      "worldUpVector",                    1),

    #(TYPE_FLOAT32,      1,      "_pad0",                        1),
    #(TYPE_FLOAT32,      1,      "_pad1",                        1),
    #(TYPE_FLOAT32,      1,      "_pad2",                        1),
    #(TYPE_FLOAT32,      1,      "_pad3",                        1),

    # for std140
    (TYPE_INT32,        4,      "instanceGeomInfoOffset",       align4(CONST["MAX_TOP_LEVEL_INSTANCE_COUNT"]) // 4),
    (TYPE_INT32,        4,      "instanceGeomInfoOffsetPrev",   align4(CONST["MAX_TOP_LEVEL_INSTANCE_COUNT"]) // 4),
    (TYPE_INT32,        4,      "instanceGeomCount",            align4(CONST["MAX_TOP_LEVEL_INSTANCE_COUNT"]) // 4),
    (TYPE_FLOAT32,     44,      "viewProjCubemap",              6),
    (TYPE_FLOAT32,     44,      "skyCubemapRotationTransform",  1),
]

GEOM_INSTANCE_STRUCT = [
    (TYPE_FLOAT32,     44,      "model",                1),
    (TYPE_FLOAT32,     44,      "prevModel",            1),
    (TYPE_FLOAT32,      4,      "materialColors",       3),
    (TYPE_UINT32,       1,      "materials0A",          1),
    (TYPE_UINT32,       1,      "materials0B",          1),
    (TYPE_UINT32,       1,      "materials0C",          1),
    (TYPE_UINT32,       1,      "materials1A",          1),
    (TYPE_UINT32,       1,      "materials1B",          1),
    (TYPE_UINT32,       1,      "materials1C",          1),
    (TYPE_UINT32,       1,      "materials2A",          1),
    (TYPE_UINT32,       1,      "materials2B",          1),
    (TYPE_UINT32,       1,      "materials2C",          1),
    (TYPE_UINT32,       1,      "flags",                1),
    (TYPE_UINT32,       1,      "baseVertexIndex",      1),
    (TYPE_UINT32,       1,      "baseIndexIndex",       1),
    (TYPE_UINT32,       1,      "prevBaseVertexIndex",  1),
    (TYPE_UINT32,       1,      "prevBaseIndexIndex",   1),
    (TYPE_UINT32,       1,      "vertexCount",          1),
    (TYPE_UINT32,       1,      "indexCount",           1),
    (TYPE_FLOAT32,      1,      "defaultRoughness",     1),
    (TYPE_FLOAT32,      1,      "defaultMetallicity",   1),
    (TYPE_FLOAT32,      1,      "defaultEmission",      1),
]

LIGHT_SPHERICAL_STRUCT = [
    (TYPE_FLOAT32,      3,      "position",             1),
    (TYPE_FLOAT32,      1,      "radius",               1),
    (TYPE_FLOAT32,      3,      "color",                1),
    (TYPE_FLOAT32,      1,      "falloff",              1),
]

LIGHT_DIRECTIONAL_STRUCT = [
    (TYPE_FLOAT32,      3,      "direction",            1),
    (TYPE_FLOAT32,      1,      "tanAngularRadius",     1),
    (TYPE_FLOAT32,      3,      "color",                1),
]

TONEMAPPING_STRUCT = [
    (TYPE_UINT32,       1,      "histogram",            CONST["COMPUTE_LUM_HISTOGRAM_BIN_COUNT"]),
    (TYPE_FLOAT32,      1,      "avgLuminance",         1),
]

VERT_PREPROC_PUSH_STRUCT = [
    (TYPE_UINT32,       1,      "tlasInstanceCount",                1),
    (TYPE_UINT32,       1,      "tlasInstanceIsDynamicBits",        align(CONST["MAX_TOP_LEVEL_INSTANCE_COUNT"], 32) // 32),
]


STRUCT_ALIGNMENT_NONE       = 0
STRUCT_ALIGNMENT_STD430     = 1
STRUCT_ALIGNMENT_STD140     = 2

STRUCT_BREAK_TYPE_NONE      = 0
STRUCT_BREAK_TYPE_COMPLEX   = 1
STRUCT_BREAK_TYPE_ONLY_C    = 2

# (structTypeName): (structDefinition, onlyForGLSL, alignmentFlags, breakComplex)
# alignmentFlags    -- if using a struct in dynamic array, it must be aligned with 16 bytes
# breakType         -- if member's type is not primitive and its count>0 then
#                      it'll be represented as an array of primitive types
STRUCTS = {
    "ShVertexBufferStatic":     (STATIC_BUFFER_STRUCT,      False,  0,                          STRUCT_BREAK_TYPE_COMPLEX),
    "ShVertexBufferDynamic":    (DYNAMIC_BUFFER_STRUCT,     False,  0,                          STRUCT_BREAK_TYPE_COMPLEX),
    "ShGlobalUniform":          (GLOBAL_UNIFORM_STRUCT,     False,  STRUCT_ALIGNMENT_STD140,    STRUCT_BREAK_TYPE_ONLY_C),
    "ShGeometryInstance":       (GEOM_INSTANCE_STRUCT,      False,  STRUCT_ALIGNMENT_STD430,    0),
    "ShTonemapping":            (TONEMAPPING_STRUCT,        False,  0,                          0),
    "ShLightSpherical":         (LIGHT_SPHERICAL_STRUCT,    False,  STRUCT_ALIGNMENT_STD430,    0),
    "ShLightDirectional":       (LIGHT_DIRECTIONAL_STRUCT,  False,  STRUCT_ALIGNMENT_STD430,    0),
    "ShVertPreprocessing":      (VERT_PREPROC_PUSH_STRUCT,  False,  0,                          0),
}

# --------------------------------------------------------------------------------------------- #
# User defined buffers: uniform, storage buffer
# --------------------------------------------------------------------------------------------- #

GETTERS = {
    # (struct type): (member to access with)
    "ShVertexBufferStatic": "staticVertices",
    "ShVertexBufferDynamic": "dynamicVertices",
}



# --------------------------------------------------------------------------------------------- #
# User defined framebuffers
# --------------------------------------------------------------------------------------------- #

FRAMEBUF_DESC_SET_NAME              = "DESC_SET_FRAMEBUFFERS"
FRAMEBUF_BASE_BINDING               = 0
FRAMEBUF_PREFIX                     = "framebuf"
FRAMEBUF_SAMPLER_POSTFIX            = "_Sampler"
FRAMEBUF_DEBUG_NAME_PREFIX          = "Framebuf "
FRAMEBUF_STORE_PREV_POSTFIX         = "_Prev"
FRAMEBUF_SAMPLER_INVALID_BINDING    = "FB_SAMPLER_INVALID_BINDING"

# only info for 2 frames are used: current and previous
FRAMEBUF_FLAGS_STORE_PREV           = 1 << 0
FRAMEBUF_FLAGS_NO_SAMPLER           = 1 << 1
FRAMEBUF_FLAGS_IS_ATTACHMENT        = 1 << 2
FRAMEBUF_FLAGS_FORCE_SIZE_1_2       = 1 << 3
FRAMEBUF_FLAGS_FORCE_SIZE_1_3       = 1 << 4
FRAMEBUF_FLAGS_FORCE_SIZE_1_4       = 1 << 5
FRAMEBUF_FLAGS_FORCE_SIZE_1_8       = 1 << 6
FRAMEBUF_FLAGS_FORCE_SIZE_1_16      = 1 << 7
FRAMEBUF_FLAGS_FORCE_SIZE_1_32      = 1 << 8
FRAMEBUF_FLAGS_BILINEAR_SAMPLER     = 1 << 9
FRAMEBUF_FLAGS_UPSCALED_SIZE        = 1 << 10
FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE    = 1 << 11
FRAMEBUF_FLAGS_USAGE_TRANSFER_DST   = 1 << 12

# only these flags are shown for C++ side
FRAMEBUF_FLAGS_ENUM = {
    "FRAMEBUF_FLAGS_IS_ATTACHMENT"      : FRAMEBUF_FLAGS_IS_ATTACHMENT,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_2"     : FRAMEBUF_FLAGS_FORCE_SIZE_1_2,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_3"     : FRAMEBUF_FLAGS_FORCE_SIZE_1_3,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_4"     : FRAMEBUF_FLAGS_FORCE_SIZE_1_4,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_8"     : FRAMEBUF_FLAGS_FORCE_SIZE_1_8,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_16"    : FRAMEBUF_FLAGS_FORCE_SIZE_1_16,
    "FRAMEBUF_FLAGS_FORCE_SIZE_1_32"    : FRAMEBUF_FLAGS_FORCE_SIZE_1_32,
    "FRAMEBUF_FLAGS_BILINEAR_SAMPLER"   : FRAMEBUF_FLAGS_BILINEAR_SAMPLER,
    "FRAMEBUF_FLAGS_UPSCALED_SIZE"      : FRAMEBUF_FLAGS_UPSCALED_SIZE,
    "FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE"  : FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE,
    "FRAMEBUF_FLAGS_USAGE_TRANSFER_DST" : FRAMEBUF_FLAGS_USAGE_TRANSFER_DST,
}

FRAMEBUFFERS = {
    # (image name)                      : (base format type, components,    flags)
    "Albedo"                            : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_IS_ATTACHMENT),
    "Normal"                            : (TYPE_UINT32,     COMPONENT_R,    FRAMEBUF_FLAGS_STORE_PREV),
    "NormalGeometry"                    : (TYPE_UINT32,     COMPONENT_R,    FRAMEBUF_FLAGS_STORE_PREV),
    "MetallicRoughness"                 : (TYPE_UNORM8,     COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "Depth"                             : (TYPE_FLOAT32,    COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "RandomSeed"                        : (TYPE_UINT32,     COMPONENT_R,    FRAMEBUF_FLAGS_STORE_PREV),
    "UnfilteredDirect"                  : (TYPE_PACK_E5,    COMPONENT_RGB,  0),
    "UnfilteredSpecular"                : (TYPE_PACK_E5,    COMPONENT_RGB,  0),
    "UnfilteredIndirectSH_R"            : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "UnfilteredIndirectSH_G"            : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "UnfilteredIndirectSH_B"            : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "SurfacePosition"                   : (TYPE_FLOAT32,    COMPONENT_RGBA, 0),
    "VisibilityBuffer"                  : (TYPE_FLOAT32,    COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "ViewDirection"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "Throughput"                        : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "Exposure"                          : (TYPE_FLOAT16,    COMPONENT_R,    FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE),
    # bilinear for bloom
    "PreFinal"                          : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
    "Final"                             : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_IS_ATTACHMENT),
    "UpscaledIntermediary"              : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_UPSCALED_SIZE),
    "UpscaledOutput"                    : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_UPSCALED_SIZE | FRAMEBUF_FLAGS_USAGE_TRANSFER_DST), # dst for DLSS

    "Motion"                            : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "MotionDlss"                        : (TYPE_FLOAT16,    COMPONENT_RG,   0),

    "AccumHistoryLength"                : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_STORE_PREV),
    
    "DiffAccumColor"                    : (TYPE_PACK_E5,    COMPONENT_RGB,  FRAMEBUF_FLAGS_STORE_PREV),
    "DiffAccumMoments"                  : (TYPE_FLOAT16,    COMPONENT_RG,   FRAMEBUF_FLAGS_STORE_PREV),
    "DiffColorHistory"                  : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "DiffPingColorAndVariance"          : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "DiffPongColorAndVariance"          : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    
    "SpecAccumColor"                    : (TYPE_PACK_E5,    COMPONENT_RGB,  FRAMEBUF_FLAGS_STORE_PREV),
    "SpecPingColor"                     : (TYPE_PACK_E5,    COMPONENT_RGB,  0),
    "SpecPongColor"                     : (TYPE_PACK_E5,    COMPONENT_RGB,  0),
    
    "IndirAccumSH_R"                    : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "IndirAccumSH_G"                    : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "IndirAccumSH_B"                    : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_STORE_PREV),
    "IndirPingSH_R"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "IndirPingSH_G"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "IndirPingSH_B"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "IndirPongSH_R"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "IndirPongSH_G"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),
    "IndirPongSH_B"                     : (TYPE_FLOAT16,    COMPONENT_RGBA, 0),

    "AtrousFilteredVariance"            : (TYPE_FLOAT16,    COMPONENT_R, 0),
    
    "GradientSamples"                   : (TYPE_UINT32,     COMPONENT_RGBA, FRAMEBUF_FLAGS_FORCE_SIZE_1_3 | FRAMEBUF_FLAGS_STORE_PREV),
    "DiffAndSpecPingGradient"           : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_FORCE_SIZE_1_3),
    "DiffAndSpecPongGradient"           : (TYPE_FLOAT16,    COMPONENT_RGBA, FRAMEBUF_FLAGS_FORCE_SIZE_1_3),
    "IndirPingGradient"                 : (TYPE_FLOAT16,    COMPONENT_R,    FRAMEBUF_FLAGS_FORCE_SIZE_1_3),
    "IndirPongGradient"                 : (TYPE_FLOAT16,    COMPONENT_R,    FRAMEBUF_FLAGS_FORCE_SIZE_1_3),

    "Bloom_Mip1"                        : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_FORCE_SIZE_1_2  | FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
    "Bloom_Mip2"                        : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_FORCE_SIZE_1_4  | FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
    "Bloom_Mip3"                        : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_FORCE_SIZE_1_8  | FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
    "Bloom_Mip4"                        : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_FORCE_SIZE_1_16 | FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
    "Bloom_Mip5"                        : (TYPE_PACK_11,    COMPONENT_RGB,  FRAMEBUF_FLAGS_FORCE_SIZE_1_32 | FRAMEBUF_FLAGS_BILINEAR_SAMPLER),
}



# ---
# User defined structs END
# ---








def getAllConstDefs(constDict):
    return "\n".join([
        "#define %s (%s)" % (name, str(value))
        for name, value in constDict.items()
    ]) + "\n\n"


def getMemberSizeStd430(baseType, dim, count):
    if dim == 1:
        return GLSL_TYPE_SIZES_STD_430[baseType] * count
    elif count == 1:
        return GLSL_TYPE_SIZES_STD_430[(baseType, dim)]
    else:
        return GLSL_TYPE_SIZES_STD_430[baseType] * dim * count


def getMemberActualSize(baseType, dim, count):
    if dim == 1:
        return TYPE_ACTUAL_SIZES[baseType] * count
    else:
        return TYPE_ACTUAL_SIZES[(baseType, dim)] * count

CURRENT_PAD_INDEX = 0

def getPadsForStruct(typeNames, uint32ToAdd):
    global CURRENT_PAD_INDEX
    r = ""
    padStr = "    " + typeNames[TYPE_UINT32] + " __pad%d;\n"
    for i in range(uint32ToAdd):
        r += padStr % (CURRENT_PAD_INDEX + i)
    CURRENT_PAD_INDEX += uint32ToAdd
    return r


# useVecMatTypes:
def getStruct(name, definition, typeNames, alignmentType, breakType):
    r = "struct " + name + "\n{\n"

    if alignmentType == STRUCT_ALIGNMENT_STD140 and typeNames == C_TYPE_NAMES:
        print("Struct \"" + name + "\" is using std140, alignment must be set manually.")

    global CURRENT_PAD_INDEX
    CURRENT_PAD_INDEX = 0

    curSize = 0
    curOffset = 0

    for baseType, dim, mname, count in definition:
        assert(count > 0)
        r += "    "

        if count == 1:
            if dim == 1:
                r +="%s %s" % (typeNames[baseType], mname)
            elif (baseType, dim) in typeNames:
                r += "%s %s" % (typeNames[(baseType, dim)], mname)
            elif dim <= 4:
                r += "%s %s[%d]" % (typeNames[baseType], mname, dim)
            else:
                if USE_MULTIDIMENSIONAL_ARRAYS_IN_C:
                    r += "%s %s[%d][%d]" % (typeNames[baseType], mname, dim // 10, dim % 10)
                else:
                    r += "%s %s[%d]" % (typeNames[baseType], mname, (dim // 10) * (dim % 10))
        else:
            #if dim > 4 and typeNames == C_TYPE_NAMES:
            #    raise Exception("If count > 1, dimensions must be in [1..4]")
            if breakType == STRUCT_BREAK_TYPE_COMPLEX or (typeNames == C_TYPE_NAMES and breakType == STRUCT_BREAK_TYPE_ONLY_C):
                if dim <= 4:
                    r += "%s %s[%d]" % (typeNames[baseType], mname, align4(count * dim))
                else:
                    r += "%s %s[%d]" % (typeNames[baseType], mname, align4(count * int(TYPE_ACTUAL_SIZES[(baseType, dim)] / 4)))
            else:
                if dim == 1:
                    r += "%s %s[%d]" % (typeNames[baseType], mname, count)
                elif (baseType, dim) in typeNames:
                    r += "%s %s[%d]" % (typeNames[(baseType, dim)], mname, count)
                else:
                    r += "%s %s[%d][%d]" % (typeNames[baseType], mname, count, dim)

        r += ";\n"

        if (alignmentType == STRUCT_ALIGNMENT_STD430):
            for i in range(count):
                memberSize = getMemberActualSize(baseType, dim, 1)
                memberAlignment = getMemberSizeStd430(baseType, dim, 1)

                alignedOffset = align(curOffset, memberAlignment)
                diff = alignedOffset - curOffset

                if diff > 0:
                    assert (diff) % 4 == 0
                    r += getPadsForStruct(typeNames, diff // 4)

                curSize += curOffset + memberSize

    if (alignmentType == STRUCT_ALIGNMENT_STD430) and curSize % 16 != 0:
        if (curSize % 16) % 4 != 0:
            raise Exception("Size of struct %s is not 4-byte aligned!" % name)
        uint32ToAdd = (align(curSize, 16) - curSize) // 4
        r += getPadsForStruct(typeNames, uint32ToAdd)

    r += "};\n"
    return r


def getAllStructDefs(typeNames):
    return "\n".join(
        getStruct(name, structDef, typeNames, alignmentType, breakType)
        for name, (structDef, onlyForGLSL, alignmentType, breakType) in STRUCTS.items()
        if not (onlyForGLSL and (typeNames == C_TYPE_NAMES))
    ) + "\n"


def capitalizeFirstLetter(s):
    return s[:1].upper() + s[1:]


# Get getter for a member with variable stride.
# Currently, stride is defined as a member in GlobalUniform
def getGLSLGetter(baseMember, baseType, dim, memberName):
    assert(2 <= dim <= 4)
    if USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE:
        strideVar = "globalUniform." + baseMember + capitalizeFirstLetter(memberName) + "Stride"
    else:
        strideVar = "globalUniform." + memberName + "Stride"

    ret = GLSL_TYPE_NAMES[(baseType, dim)] + "(\n        "
    for i in range(dim):
        ret += "%s.%s[index * %s + %d]" % (baseMember, memberName, strideVar, i)
        if i != dim - 1:
            ret += ",\n        "
    ret += ");"

    res = "%s get%s%s(uint index)\n{\n" \
        "    return " + ret + "\n}\n"

    return res % (
        GLSL_TYPE_NAMES[(baseType, dim)],
        capitalizeFirstLetter(baseMember), capitalizeFirstLetter(memberName),
    )


def getAllGLSLGetters():
    return "\n".join(
        getGLSLGetter(baseMember, baseType, dim, mname)
        for structType, baseMember in GETTERS.items()
        # for each member in struct
        for baseType, dim, mname, count in STRUCTS[structType][0]
        # if using variableStride
        if count > 1 and dim > 1
    ) + "\n"


def getGLSLSetter(baseMember, baseType, dim, memberName):
    assert(2 <= dim <= 4)
    if USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE:
        strideVar = "globalUniform." + baseMember + capitalizeFirstLetter(memberName) + "Stride"
    else:
        strideVar = "globalUniform." + memberName + "Stride"

    st = ""
    for i in range(dim):
        st += "    %s.%s[index * %s + %d] = value[%d];\n" % (baseMember, memberName, strideVar, i, i)

    res = "void set%s%s(uint index, %s value)\n{\n%s}\n"

    return res % (
        capitalizeFirstLetter(baseMember), capitalizeFirstLetter(memberName),
        GLSL_TYPE_NAMES[(baseType, dim)],
        st
    )


def getAllGLSLSetters():
    return "\n".join(
        getGLSLSetter(baseMember, baseType, dim, mname)
        for structType, baseMember in GETTERS.items()
        # for each member in struct
        for baseType, dim, mname, count in STRUCTS[structType][0]
        # if using variableStride
        if count > 1 and dim > 1
    ) + "\n"


CURRENT_FRAMEBUF_BINDING_COUNT = 0

def getGLSLFramebufDeclaration(name, baseFormat, components, flags):
    global CURRENT_FRAMEBUF_BINDING_COUNT

    binding = FRAMEBUF_BASE_BINDING + CURRENT_FRAMEBUF_BINDING_COUNT
    bindingSampler = binding + 1
    CURRENT_FRAMEBUF_BINDING_COUNT += 1

    template = ("layout(set = %s, binding = %d, %s) uniform %s %s;")

    r = template % (FRAMEBUF_DESC_SET_NAME, binding, 
        GLSL_IMAGE_FORMATS[(baseFormat, components)], 
        GLSL_IMAGE_2D_TYPE[baseFormat], name)

    if flags & FRAMEBUF_FLAGS_STORE_PREV:
        r += "\n"
        r += getGLSLFramebufDeclaration(
            name + FRAMEBUF_STORE_PREV_POSTFIX, baseFormat, components, 
            flags & ~FRAMEBUF_FLAGS_STORE_PREV)

    return r


def getGLSLFramebufSamplerDeclaration(name, baseFormat, components, flags):
    global CURRENT_FRAMEBUF_BINDING_COUNT

    binding = FRAMEBUF_BASE_BINDING + CURRENT_FRAMEBUF_BINDING_COUNT - 1
    bindingSampler = binding + 1
    CURRENT_FRAMEBUF_BINDING_COUNT += 1

    templateSampler = ("layout(set = %s, binding = %d) uniform %s %s;")

    r = templateSampler % (FRAMEBUF_DESC_SET_NAME, bindingSampler,
        GLSL_SAMPLER_2D_TYPE[baseFormat], name + FRAMEBUF_SAMPLER_POSTFIX)

    if flags & FRAMEBUF_FLAGS_STORE_PREV:
        r += "\n"
        r += getGLSLFramebufSamplerDeclaration(
            name + FRAMEBUF_STORE_PREV_POSTFIX, baseFormat, components, 
            flags & ~FRAMEBUF_FLAGS_STORE_PREV)

    return r


def getGLSLFramebufPackUnpackE5(name, withPrev):
    templateImgStore = ("void imageStore%s(const ivec2 pix, const vec3 unpacked) "
                        "{ imageStore(%s, pix, uvec4(encodeE5B9G9R9(unpacked))); }")
    templateTxlFetch = ("vec3 texelFetch%s(const ivec2 pix)"
                        "{ return decodeE5B9G9R9(texelFetch(%s, pix, 0).r); }")
    r  = templateImgStore % (name, FRAMEBUF_PREFIX + name) + "\n"
    r += templateTxlFetch % (name, FRAMEBUF_PREFIX + name + FRAMEBUF_SAMPLER_POSTFIX) + "\n"
    if withPrev:
        r += templateTxlFetch % (name + FRAMEBUF_STORE_PREV_POSTFIX, FRAMEBUF_PREFIX + name + FRAMEBUF_STORE_PREV_POSTFIX + FRAMEBUF_SAMPLER_POSTFIX) + "\n"
    return r
    

def getAllGLSLFramebufDeclarations():
    global CURRENT_FRAMEBUF_BINDING_COUNT
    CURRENT_FRAMEBUF_BINDING_COUNT = 0
    return "#ifdef " + FRAMEBUF_DESC_SET_NAME \
        + "\n\n// framebuffers\n" \
        + "\n".join(
            getGLSLFramebufDeclaration(FRAMEBUF_PREFIX + name, baseFormat, components, flags)
            for name, (baseFormat, components, flags) in FRAMEBUFFERS.items()
        ) \
        \
        + "\n\n// samplers\n" \
        + "\n".join(
            getGLSLFramebufSamplerDeclaration(FRAMEBUF_PREFIX + name, baseFormat, components, flags)
            for name, (baseFormat, components, flags) in FRAMEBUFFERS.items()
            if not (flags & FRAMEBUF_FLAGS_NO_SAMPLER)
        ) \
        \
        + "\n\n// pack/unpack formats\n" \
        + "\n".join(
            getGLSLFramebufPackUnpackE5(name, flags & FRAMEBUF_FLAGS_STORE_PREV)
            for name, (baseFormat, components, flags) in FRAMEBUFFERS.items()
            if baseFormat == TYPE_PACK_E5 and not (flags & FRAMEBUF_FLAGS_NO_SAMPLER)
        ) \
        \
        + "\n\n#endif\n"


def removeCoupledDuplicateChars(str, charToRemove = '_'):
    r = ""
    for i in range(0, len(str)):
        if i == 0 or str[i] != str[i - 1] or str[i] != charToRemove:
            r += str[i]
    return r


# make all letters capital and insert "_" before 
# capital letters in the original string
def capitalizeForEnum(s):
    return removeCoupledDuplicateChars("_".join(filter(None, re.split("([A-Z][^A-Z]*)", s))).upper())


def getAllFramebufConstants():
    names = []
    for name, (_, _, flags) in FRAMEBUFFERS.items():
        names.append(name)
        if flags & FRAMEBUF_FLAGS_STORE_PREV:
            names.append(name + FRAMEBUF_STORE_PREV_POSTFIX)

    fbConst = "#define " + FRAMEBUF_SAMPLER_INVALID_BINDING + " 0xFFFFFFFF\n\n"

    fbEnum = "enum FramebufferImageIndex\n{\n" + "\n".join(
        "    FB_IMAGE_INDEX_%s = %d," % (capitalizeForEnum(names[i]), i)
        for i in range(len(names))
    ) + "\n};\n\n"

    fbFlags = "enum FramebufferImageFlagBits\n{\n" + "\n".join(
        "    FB_IMAGE_FLAGS_%s = %d," % (flName, flValue)
        for (flName, flValue) in FRAMEBUF_FLAGS_ENUM.items()
    ) + "\n};\ntypedef uint32_t FramebufferImageFlags;\n\n"

    return fbConst + fbEnum + fbFlags


def getPublicFlags(flags):
    r = " | ".join(
        "RTGL1::FB_IMAGE_FLAGS_" + flName
        for (flName, flValue) in FRAMEBUF_FLAGS_ENUM.items()
        if flValue & flags
    )
    if r == "":
        return "0"
    else:
        return r
    

def getAllVulkanFramebufDeclarations():
    return ("extern const uint32_t ShFramebuffers_Count;\n"
            "extern const VkFormat ShFramebuffers_Formats[];\n"
            "extern const FramebufferImageFlags ShFramebuffers_Flags[];\n"
            "extern const uint32_t ShFramebuffers_Bindings[];\n"
            "extern const uint32_t ShFramebuffers_BindingsSwapped[];\n"
            "extern const uint32_t ShFramebuffers_Sampler_Bindings[];\n"
            "extern const uint32_t ShFramebuffers_Sampler_BindingsSwapped[];\n"
            "extern const char *const ShFramebuffers_DebugNames[];\n\n")


def getAllVulkanFramebufDefinitions():
    template = ("const uint32_t RTGL1::ShFramebuffers_Count = %d;\n\n"
                "const VkFormat RTGL1::ShFramebuffers_Formats[] = \n{\n%s};\n\n"
                "const RTGL1::FramebufferImageFlags RTGL1::ShFramebuffers_Flags[] = \n{\n%s};\n\n"
                "const uint32_t RTGL1::ShFramebuffers_Bindings[] = \n{\n%s};\n\n"
                "const uint32_t RTGL1::ShFramebuffers_BindingsSwapped[] = \n{\n%s};\n\n"
                "const uint32_t RTGL1::ShFramebuffers_Sampler_Bindings[] = \n{\n%s};\n\n"
                "const uint32_t RTGL1::ShFramebuffers_Sampler_BindingsSwapped[] = \n{\n%s};\n\n"
                "const char *const RTGL1::ShFramebuffers_DebugNames[] = \n{\n%s};\n\n")
    TAB_STR = "    "
    formats = ""
    count = 0
    publicFlags = ""
    samplerCount = 0
    bindings = ""    
    bindingsSwapped = ""
    samplerBindings = ""
    samplerBindingsSwapped = ""
    names = ""
    for name, (baseFormat, components, flags) in FRAMEBUFFERS.items():
        formats += TAB_STR + VULKAN_IMAGE_FORMATS[(baseFormat, components)] + ",\n"
        names += TAB_STR + "\"" + FRAMEBUF_DEBUG_NAME_PREFIX + name + "\",\n"
        publicFlags += TAB_STR + getPublicFlags(flags) + ",\n"

        if not flags & FRAMEBUF_FLAGS_STORE_PREV:
            bindings                += TAB_STR + str(count)         + ",\n"
            bindingsSwapped         += TAB_STR + str(count)         + ",\n"
        else:
            bindings                += TAB_STR + str(count)         + ",\n"
            bindings                += TAB_STR + str(count + 1)     + ",\n"
            bindingsSwapped         += TAB_STR + str(count + 1)     + ",\n"
            bindingsSwapped         += TAB_STR + str(count)         + ",\n"
            
            formats += TAB_STR + VULKAN_IMAGE_FORMATS[(baseFormat, components)] + ",\n"
            names += TAB_STR + "\"" + FRAMEBUF_DEBUG_NAME_PREFIX + name + FRAMEBUF_STORE_PREV_POSTFIX + "\",\n"
            publicFlags += TAB_STR + getPublicFlags(flags) + ",\n"
            count += 1

        count += 1

    for name, (baseFormat, components, flags) in FRAMEBUFFERS.items():
        bindingIndex     = str(count + samplerCount)
        bindingIndexNext = str(count + samplerCount + 1)
        
        if flags & FRAMEBUF_FLAGS_NO_SAMPLER:
            bindingIndex = bindingIndexNext = FRAMEBUF_SAMPLER_INVALID_BINDING

        if not flags & FRAMEBUF_FLAGS_STORE_PREV:
            samplerBindings         += TAB_STR + bindingIndex       + ",\n"
            samplerBindingsSwapped  += TAB_STR + bindingIndex       + ",\n"
        else:
            samplerBindings         += TAB_STR + bindingIndex       + ",\n"
            samplerBindings         += TAB_STR + bindingIndexNext   + ",\n"
            samplerBindingsSwapped  += TAB_STR + bindingIndexNext   + ",\n"
            samplerBindingsSwapped  += TAB_STR + bindingIndex       + ",\n"
            samplerCount += 1

        samplerCount += 1

    return template % (count, formats, publicFlags, bindings, bindingsSwapped, samplerBindings, samplerBindingsSwapped, names)


FILE_HEADER = "// This file was generated by GenerateShaderCommon.py\n\n"


def writeToC(commonHeaderFile, fbHeaderFile, fbSourceFile):
    commonHeaderFile.write(FILE_HEADER)
    commonHeaderFile.write("#pragma once\n\n")
    commonHeaderFile.write("namespace RTGL1\n{\n\n")
    commonHeaderFile.write("#include <stdint.h>\n\n")
    commonHeaderFile.write(getAllConstDefs(CONST))
    commonHeaderFile.write(getAllStructDefs(C_TYPE_NAMES))
    commonHeaderFile.write("}")

    fbHeaderFile.write(FILE_HEADER)
    fbHeaderFile.write("#pragma once\n\n")
    fbHeaderFile.write("#include \"../Common.h\"\n\n")
    fbHeaderFile.write("namespace RTGL1\n{\n\n")
    fbHeaderFile.write(getAllFramebufConstants())
    fbHeaderFile.write(getAllVulkanFramebufDeclarations())
    fbHeaderFile.write("}")

    fbSourceFile.write(FILE_HEADER)
    fbSourceFile.write("#include \"%s\"\n\n" % os.path.basename(fbHeaderFile.name))
    fbSourceFile.write(getAllVulkanFramebufDefinitions())


def writeToGLSL(f, generateGetSet):
    f.write(FILE_HEADER)
    f.write(getAllConstDefs(CONST))
    f.write(getAllConstDefs(CONST_GLSL_ONLY))
    f.write(getAllStructDefs(GLSL_TYPE_NAMES))
    if generateGetSet:
        f.write(getAllGLSLGetters())
        f.write(getAllGLSLSetters())
    f.write(getAllGLSLFramebufDeclarations())


def main():
    generateGetSet = False
    basePath = ""

    for i in range(len(sys.argv)):
        if "--help" == sys.argv[i] or "-help" == sys.argv[i]:
            print("--getset   : generate getters and setters for non-trivial members")
            print("--path     : specify path to target folder in the next argument")
            return
        if "--getset" == sys.argv[i]:
            generateGetSet = True
        if "--path" == sys.argv[i]:
            if i + 1 < len(sys.argv):
                basePath = sys.argv[i + 1]
                if not os.path.exists(basePath):
                    print("Folder with path \"" + basePath + "\" doesn't exist.")
                    return
            else:
                print("--path expects folder path in the next argument.")
                return

    evalConst()
    # with open('ShaderConfig.csv', newline='') as csvfile:
    with open(basePath + "ShaderCommonC.h", "w") as commonHeaderFile:
        with open(basePath + "ShaderCommonCFramebuf.h", "w") as fbHeaderFile:
            with open(basePath + "ShaderCommonCFramebuf.cpp", "w") as fbSourceFile:
                writeToC(commonHeaderFile, fbHeaderFile, fbSourceFile)
    with open(basePath + "ShaderCommonGLSL.h", "w") as f:
        writeToGLSL(f, generateGetSet)

# main
if __name__ == "__main__":
    main()
