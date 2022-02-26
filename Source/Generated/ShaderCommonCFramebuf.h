// This file was generated by GenerateShaderCommon.py

#pragma once

#include "../Common.h"

namespace RTGL1
{

#define FB_SAMPLER_INVALID_BINDING 0xFFFFFFFF

enum FramebufferImageIndex
{
    FB_IMAGE_INDEX_ALBEDO = 0,
    FB_IMAGE_INDEX_NORMAL = 1,
    FB_IMAGE_INDEX_NORMAL_PREV = 2,
    FB_IMAGE_INDEX_NORMAL_GEOMETRY = 3,
    FB_IMAGE_INDEX_NORMAL_GEOMETRY_PREV = 4,
    FB_IMAGE_INDEX_METALLIC_ROUGHNESS = 5,
    FB_IMAGE_INDEX_METALLIC_ROUGHNESS_PREV = 6,
    FB_IMAGE_INDEX_DEPTH = 7,
    FB_IMAGE_INDEX_DEPTH_PREV = 8,
    FB_IMAGE_INDEX_RANDOM_SEED = 9,
    FB_IMAGE_INDEX_RANDOM_SEED_PREV = 10,
    FB_IMAGE_INDEX_MOTION = 11,
    FB_IMAGE_INDEX_UNFILTERED_DIRECT = 12,
    FB_IMAGE_INDEX_UNFILTERED_SPECULAR = 13,
    FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_R = 14,
    FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_G = 15,
    FB_IMAGE_INDEX_UNFILTERED_INDIRECT_S_H_B = 16,
    FB_IMAGE_INDEX_SURFACE_POSITION = 17,
    FB_IMAGE_INDEX_VISIBILITY_BUFFER = 18,
    FB_IMAGE_INDEX_VISIBILITY_BUFFER_PREV = 19,
    FB_IMAGE_INDEX_SECTOR_INDEX = 20,
    FB_IMAGE_INDEX_SECTOR_INDEX_PREV = 21,
    FB_IMAGE_INDEX_VIEW_DIRECTION = 22,
    FB_IMAGE_INDEX_PRIMARY_TO_REFL_REFR = 23,
    FB_IMAGE_INDEX_THROUGHPUT = 24,
    FB_IMAGE_INDEX_PRE_FINAL = 25,
    FB_IMAGE_INDEX_FINAL = 26,
    FB_IMAGE_INDEX_UPSCALED_PING = 27,
    FB_IMAGE_INDEX_UPSCALED_PONG = 28,
    FB_IMAGE_INDEX_DEPTH_DLSS = 29,
    FB_IMAGE_INDEX_MOTION_DLSS = 30,
    FB_IMAGE_INDEX_ACCUM_HISTORY_LENGTH = 31,
    FB_IMAGE_INDEX_ACCUM_HISTORY_LENGTH_PREV = 32,
    FB_IMAGE_INDEX_DIFF_ACCUM_COLOR = 33,
    FB_IMAGE_INDEX_DIFF_ACCUM_COLOR_PREV = 34,
    FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS = 35,
    FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS_PREV = 36,
    FB_IMAGE_INDEX_DIFF_COLOR_HISTORY = 37,
    FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE = 38,
    FB_IMAGE_INDEX_DIFF_PONG_COLOR_AND_VARIANCE = 39,
    FB_IMAGE_INDEX_SPEC_ACCUM_COLOR = 40,
    FB_IMAGE_INDEX_SPEC_ACCUM_COLOR_PREV = 41,
    FB_IMAGE_INDEX_SPEC_PING_COLOR = 42,
    FB_IMAGE_INDEX_SPEC_PONG_COLOR = 43,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_R = 44,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_R_PREV = 45,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_G = 46,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_G_PREV = 47,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_B = 48,
    FB_IMAGE_INDEX_INDIR_ACCUM_S_H_B_PREV = 49,
    FB_IMAGE_INDEX_INDIR_PING_S_H_R = 50,
    FB_IMAGE_INDEX_INDIR_PING_S_H_G = 51,
    FB_IMAGE_INDEX_INDIR_PING_S_H_B = 52,
    FB_IMAGE_INDEX_INDIR_PONG_S_H_R = 53,
    FB_IMAGE_INDEX_INDIR_PONG_S_H_G = 54,
    FB_IMAGE_INDEX_INDIR_PONG_S_H_B = 55,
    FB_IMAGE_INDEX_ATROUS_FILTERED_VARIANCE = 56,
    FB_IMAGE_INDEX_BLOOM_MIP1 = 57,
    FB_IMAGE_INDEX_BLOOM_MIP2 = 58,
    FB_IMAGE_INDEX_BLOOM_MIP3 = 59,
    FB_IMAGE_INDEX_BLOOM_MIP4 = 60,
    FB_IMAGE_INDEX_BLOOM_MIP5 = 61,
    FB_IMAGE_INDEX_BLOOM_RESULT = 62,
    FB_IMAGE_INDEX_WIPE_EFFECT_SOURCE = 63,
    FB_IMAGE_INDEX_GRADIENT_SAMPLES = 64,
    FB_IMAGE_INDEX_GRADIENT_SAMPLES_PREV = 65,
    FB_IMAGE_INDEX_DIFF_AND_SPEC_PING_GRADIENT = 66,
    FB_IMAGE_INDEX_DIFF_AND_SPEC_PONG_GRADIENT = 67,
    FB_IMAGE_INDEX_INDIR_PING_GRADIENT = 68,
    FB_IMAGE_INDEX_INDIR_PONG_GRADIENT = 69,
};

enum FramebufferImageFlagBits
{
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_IS_ATTACHMENT = 4,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_2 = 8,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_3 = 16,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_4 = 32,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_8 = 64,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_16 = 128,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_FORCE_SIZE_1_32 = 256,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_BILINEAR_SAMPLER = 512,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_UPSCALED_SIZE = 1024,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_SINGLE_PIXEL_SIZE = 2048,
    FB_IMAGE_FLAGS_FRAMEBUF_FLAGS_USAGE_TRANSFER = 4096,
};
typedef uint32_t FramebufferImageFlags;

extern const uint32_t ShFramebuffers_Count;
extern const VkFormat ShFramebuffers_Formats[];
extern const FramebufferImageFlags ShFramebuffers_Flags[];
extern const uint32_t ShFramebuffers_Bindings[];
extern const uint32_t ShFramebuffers_BindingsSwapped[];
extern const uint32_t ShFramebuffers_Sampler_Bindings[];
extern const uint32_t ShFramebuffers_Sampler_BindingsSwapped[];
extern const char *const ShFramebuffers_DebugNames[];

}