// This file was generated by GenerateShaderCommon.py

#include "../Common.h"

enum FramebufferImageIndex
{
    FB_IMAGE_ALBEDO = 0,
    FB_IMAGE_NORMAL = 1,
    FB_IMAGE_NORMAL_PREV = 2,
    FB_IMAGE_NORMAL_GEOMETRY = 3,
    FB_IMAGE_NORMAL_GEOMETRY_PREV = 4,
    FB_IMAGE_METALLIC_ROUGHNESS = 5,
    FB_IMAGE_METALLIC_ROUGHNESS_PREV = 6,
    FB_IMAGE_DEPTH = 7,
    FB_IMAGE_DEPTH_PREV = 8,
    FB_IMAGE_RANDOM_SEED = 9,
    FB_IMAGE_RANDOM_SEED_PREV = 10,
    FB_IMAGE_LIGHT_DIRECT_DIFFUSE = 11,
    FB_IMAGE_LIGHT_DIRECT_DIFFUSE_PREV = 12,
    FB_IMAGE_LIGHT_DIRECT_SPECULAR = 13,
    FB_IMAGE_LIGHT_DIRECT_SPECULAR_PREV = 14,
    FB_IMAGE_SURFACE_POSITION = 15,
    FB_IMAGE_VIEW_DIRECTION = 16,
};

extern const uint32_t ShFramebuffers_Count;
extern const VkFormat ShFramebuffers_Formats[];
extern const uint32_t ShFramebuffers_Bindings[];
extern const uint32_t ShFramebuffers_BindingsSwapped[];
extern const uint32_t ShFramebuffers_Sampler_Bindings[];
extern const uint32_t ShFramebuffers_Sampler_BindingsSwapped[];
extern const char *const ShFramebuffers_DebugNames[];

