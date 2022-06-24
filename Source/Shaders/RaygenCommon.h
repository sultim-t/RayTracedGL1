// Copyright (c) 2021 Sultim Tsyrendashiev
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

#ifndef RAYGEN_COMMON_H_
#define RAYGEN_COMMON_H_

#extension GL_EXT_ray_tracing : require



#define FRONT_FACE_IS_PRIMARY true



#include "ShaderCommonGLSLFunc.h"



#if !defined(DESC_SET_TLAS) || \
    !defined(DESC_SET_GLOBAL_UNIFORM) || \
    !defined(DESC_SET_VERTEX_DATA) || \
    !defined(DESC_SET_TEXTURES) || \
    !defined(DESC_SET_RANDOM) || \
    !defined(DESC_SET_LIGHT_SOURCES)
        #error Descriptor set indices must be set!
#endif


#include "Light.h"
#include "LightGrid.h"
#include "Media.h"
#include "RayCone.h"

#define GET_TARGET_PDF targetPdfForLightSample
#include "Reservoir.h"

#define HITINFO_INL_PRIM
    #include "HitInfo.inl"
#undef HITINFO_INL_PRIM

#define HITINFO_INL_RFL
    #include "HitInfo.inl"
#undef HITINFO_INL_RFL

#define HITINFO_INL_INDIR
    #include "HitInfo.inl"
#undef HITINFO_INL_INDIR

#define HITINFO_INL_RFR
    #include "HitInfo.inl"
#undef HITINFO_INL_RFR



layout(set = DESC_SET_TLAS, binding = BINDING_ACCELERATION_STRUCTURE_MAIN)   uniform accelerationStructureEXT topLevelAS;

#ifdef DESC_SET_CUBEMAPS
layout(set = DESC_SET_CUBEMAPS, binding = BINDING_CUBEMAPS) uniform samplerCube globalCubemaps[];
#endif
#ifdef DESC_SET_RENDER_CUBEMAP
layout(set = DESC_SET_RENDER_CUBEMAP, binding = BINDING_RENDER_CUBEMAP) uniform samplerCube renderCubemap;
#endif


layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload g_payload;

#ifdef RAYGEN_SHADOW_PAYLOAD
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayloadShadow g_payloadShadow;
#endif // RAYGEN_SHADOW_PAYLOAD



uint getPrimaryVisibilityCullMask()
{
    return globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT | INSTANCE_MASK_FIRST_PERSON;
}

uint getReflectionRefractionCullMask(uint surfInstCustomIndex, uint geometryInstanceFlags, bool isRefraction)
{
    uint world = globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT;

    if ((geometryInstanceFlags & GEOM_INST_FLAG_IGNORE_REFL_REFR_AFTER) != 0)
    {
        // ignore refl/refr geometry if requested
        world = world & (~INSTANCE_MASK_REFLECT_REFRACT);
    }

    if ((surfInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON) != 0)
    {
        // ignore first-person viewer -- on first-person
        return world | INSTANCE_MASK_FIRST_PERSON;
    }
    
    return isRefraction ? 
        // no first-person viewer in refractions
        world | INSTANCE_MASK_FIRST_PERSON :
        // no first-person in reflections
        world | INSTANCE_MASK_FIRST_PERSON_VIEWER;
}

uint getShadowCullMask(uint surfInstCustomIndex)
{
    const uint world = 
        globalUniform.rayCullMaskWorld_Shadow | 
        (globalUniform.enableShadowsFromReflRefr == 0 ? 0 : INSTANCE_MASK_REFLECT_REFRACT);

    if ((surfInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON) != 0)
    {
        // no first-person viewer shadows -- on first-person
        return world | INSTANCE_MASK_FIRST_PERSON;
    }
    else if ((surfInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER) != 0)
    {
        // no first-person shadows -- on first-person viewer
        return world | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    else
    {
        // no first-person shadows -- on world
        return world | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
}

uint getIndirectIlluminationCullMask(uint surfInstCustomIndex)
{
    const uint world = 
        globalUniform.rayCullMaskWorld | 
        (globalUniform.enableIndirectFromReflRefr == 0 ? 0 : INSTANCE_MASK_REFLECT_REFRACT);
    
    if ((surfInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON) != 0)
    {
        // no first-person viewer indirect illumination -- on first-person
        return world | INSTANCE_MASK_FIRST_PERSON;
    }
    else if ((surfInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER) != 0)
    {
        // no first-person indirect illumination -- on first-person viewer
        return world | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    else
    {
        // no first-person indirect illumination -- on first-person viewer
        return world | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
}



uint getAdditionalRayFlags()
{
    return globalUniform.rayCullBackFaces != 0 ? gl_RayFlagsCullFrontFacingTrianglesEXT : 0;
}



bool doesPayloadContainHitInfo(const ShPayload p)
{
    if (p.instIdAndIndex == UINT32_MAX || p.geomAndPrimIndex == UINT32_MAX)
    {
        return false;
    }

    int instanceId, instanceCustomIndex;
    unpackInstanceIdAndCustomIndex(p.instIdAndIndex, instanceId, instanceCustomIndex);

    if ((instanceCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_SKY) != 0)
    {
        return false;
    }

    return true;
}

void resetPayload()
{
    g_payload.baryCoords = vec2(0.0);
    g_payload.instIdAndIndex = UINT32_MAX;
    g_payload.geomAndPrimIndex = UINT32_MAX;
}

ShPayload tracePrimaryRay(vec3 origin, vec3 direction)
{
    resetPayload();

    uint cullMask = getPrimaryVisibilityCullMask();

    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, globalUniform.primaryRayMinDist, direction, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT);

    return g_payload; 
}

ShPayload traceReflectionRefractionRay(vec3 origin, vec3 direction, uint surfInstCustomIndex, uint geometryInstanceFlags, bool isRefraction)
{
    resetPayload();

    uint cullMask = getReflectionRefractionCullMask(surfInstCustomIndex, geometryInstanceFlags, isRefraction);

    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, 0.001, direction, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT);

    return g_payload; 
}

ShPayload traceIndirectRay(uint surfInstCustomIndex, vec3 surfPosition, vec3 bounceDirection)
{
    resetPayload();

    uint cullMask = getIndirectIlluminationCullMask(surfInstCustomIndex);

    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        surfPosition, 0.001, bounceDirection, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT); 

    return g_payload;
}



#ifdef DESC_SET_CUBEMAPS
// Get sky color for primary visibility, i.e. without skyColorMultiplier
vec3 getSkyPrimary(vec3 direction)
{
    uint skyType = globalUniform.skyType;

#ifdef DESC_SET_RENDER_CUBEMAP
    if (skyType == SKY_TYPE_RASTERIZED_GEOMETRY)
    {
        return texture(renderCubemap, direction).rgb;
    }
#endif

    if (skyType == SKY_TYPE_CUBEMAP)
    {
        direction = mat3(globalUniform.skyCubemapRotationTransform) * direction;
        
        return texture(globalCubemaps[nonuniformEXT(globalUniform.skyCubemapIndex)], direction).rgb;
    }

    return globalUniform.skyColorDefault.xyz;
}

vec3 getSky(vec3 direction)
{
    vec3 col = getSkyPrimary(direction);
    float l = getLuminance(col);

    return mix(vec3(l), col, globalUniform.skyColorSaturation) * globalUniform.skyColorMultiplier;
}
#endif



#ifdef RAYGEN_SHADOW_PAYLOAD



struct LightResult
{
    vec3    diffuse;
    bool    shadowRayEnable;
    vec3    specular;
    bool    shadowRayIgnoreFirstPersonViewer;
    vec3    shadowRayEnd;
};

LightResult newLightResult()
{
    LightResult r;
    r.diffuse           = vec3(0);
    r.specular          = vec3(0);
    r.shadowRayEnd      = vec3(0);
    r.shadowRayEnable   = false;
    r.shadowRayIgnoreFirstPersonViewer = false;
    
    return r;
}



#define SHADOW_RAY_EPS       0.01
#define RAY_ORIGIN_LEAK_BIAS 0.01    // offset a bit towards a viewer to prevent light leaks from the other side of polygons



bool traceShadowRay(uint surfInstCustomIndex, vec3 start, vec3 end, bool ignoreFirstPersonViewer /* = false */)
{
    // prepare shadow payload
    g_payloadShadow.isShadowed = 1;  

    uint cullMask = getShadowCullMask(surfInstCustomIndex);

    if (ignoreFirstPersonViewer)
    {
        cullMask &= ~INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }

    vec3 l = end - start;
    float maxDistance = length(l);
    l /= maxDistance;

    traceRayEXT(
        topLevelAS, 
        gl_RayFlagsSkipClosestHitShaderEXT | getAdditionalRayFlags(), 
        cullMask, 
        0, 0, 	// sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_SHADOW, 		// shadow missIndex
        start, 0.001, l, maxDistance - SHADOW_RAY_EPS, 
        PAYLOAD_INDEX_SHADOW);

    return g_payloadShadow.isShadowed == 1;
}



void shade(const Surface surf, const LightSample light, float oneOverPdf, out vec3 diffuse, out vec3 specular)
{
    vec3 l = safeNormalize(light.position - surf.position);
    float nl = dot(surf.normal, l);
    float ngl = dot(surf.normalGeom, l);

    if (nl <= 0 || ngl <= 0)
    {
        diffuse = specular = vec3(0);
        return;
    }

    diffuse  = light.dw * nl * light.color * evalBRDFLambertian(1.0);
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    specular = light.dw * nl * light.color * evalBRDFSmithGGX(surf.normal, surf.toViewerDir, l, surf.roughness, surf.specularColor);
#endif

    diffuse  *= oneOverPdf;
    specular *= oneOverPdf;
}

void shade(const Surface surf, const Reservoir reservoir, const vec2 pointRnd, out vec3 diffuse, out vec3 specular)
{
    if (!isReservoirValid(reservoir))
    {
        diffuse = specular = vec3(0.0);
        return;
    }

    const LightSample l = sampleLight(lightSources[reservoir.selected], surf.position, pointRnd);
    shade(surf, l, calcSelectedSampleWeight(reservoir), diffuse, specular);
}

float targetPdfForLightSample(const LightSample light, const Surface surf)
{
    vec3 d, s;
    shade(surf, light, 1.0, d, s);

    return getLuminance(d + s);
}

float targetPdfForLightSample(uint lightIndex, const Surface surf, const vec2 pointRnd)
{
    const LightSample light = sampleLight(lightSources[lightIndex], surf.position, pointRnd);
    return targetPdfForLightSample(light, surf);
}



#ifdef RAYGEN_FORCE_SIMPLE_LIGHT_SAMPLE_METHOD
    #define LIGHT_SAMPLE_METHOD 0
#else
    #define LIGHT_SAMPLE_METHOD 2
#endif

#define INITIAL_SAMPLES 8
#define TEMPORAL_SAMPLES 1
#define TEMPORAL_RADIUS 2
#define SPATIAL_SAMPLES 8
#define SPATIAL_RADIUS 30

bool testSurfaceForReuse(
    const ivec3 curChRenderArea, const ivec2 otherPix,
    float curDepth, float otherDepth,
    const vec3 curNormal, const vec3 otherNormal)
{
    #define DEPTH_THRESHOLD 0.1
    #define NORMAL_THRESHOLD 0.5

    return 
        testPixInRenderArea(otherPix, curChRenderArea) &&
        (abs(curDepth - otherDepth) / abs(curDepth) < DEPTH_THRESHOLD) &&
        (dot(curNormal, otherNormal) > NORMAL_THRESHOLD);
}

// TODO: remove from here
Reservoir chooseLight(const ivec2 pix, uint seed, const Surface surf, const vec2 pointRnd)
{
    const ivec3 chRenderArea = getCheckerboardedRenderArea(pix); // assuming that pix is checkerboarded
    const float motionZ = texelFetch(framebufMotion_Sampler, pix, 0).z;
    const float depthCur = texelFetch(framebufDepth_Sampler, pix, 0).r;
    const vec2 posPrev = getPrevScreenPos(framebufMotion_Sampler, pix);


    Reservoir initReservoir = imageLoadReservoirInitial(pix);
    
    Reservoir combined;
    initCombinedReservoir(
        combined, 
        initReservoir);


    // temporal
    for (int pixIndex = 0; pixIndex < TEMPORAL_SAMPLES; pixIndex++)
    {
        vec2 rndOffset = rndBlueNoise8(seed, 0 + pixIndex).xy * 2.0 - 1.0;
        ivec2 pp = ivec2(floor(posPrev + rndOffset * TEMPORAL_RADIUS));

        {
            const float depthPrev = texelFetch(framebufDepth_Prev_Sampler, pp, 0).r;
            const vec3 normalPrev = texelFetchNormal_Prev(pp);

            if (!testSurfaceForReuse(chRenderArea, pp, 
                                     depthCur, depthPrev - motionZ,
                                     surf.normal, normalPrev))
            {
                continue;
            }
        }

        Reservoir temporal = imageLoadReservoir_Prev(pp);
        // renormalize to prevent precision problems
        normalizeReservoir(temporal, initReservoir.M * 20);

        float temporalTargetPdf_curSurf = 0.0;
        if (temporal.selected != LIGHT_INDEX_NONE)
        {
            uint selected_curFrame = lightSources_Index_PrevToCur[temporal.selected];

            if (selected_curFrame != UINT32_MAX && selected_curFrame != LIGHT_INDEX_NONE)
            {
                temporalTargetPdf_curSurf = targetPdfForLightSample(selected_curFrame, surf, pointRnd);
                temporal.selected = selected_curFrame;
            }
        }

        float rnd = rnd16(seed, 12 + pixIndex);
        updateCombinedReservoir_newSurf(
            combined, 
            temporal, temporalTargetPdf_curSurf, rnd);
    } 

    for (int pixIndex = 0; pixIndex < SPATIAL_SAMPLES; pixIndex++)
    {
        vec2 rndOffset = rndBlueNoise8(seed, 85 + pixIndex).xy * 2.0 - 1.0;
        ivec2 pp = pix + ivec2(rndOffset * SPATIAL_RADIUS);

        {
            const float depthOther = texelFetch(framebufDepth_Sampler, pp, 0).r;
            const vec3 normalOther = texelFetchNormal(pp);

            if (!testSurfaceForReuse(chRenderArea, pp, 
                                     depthCur, depthOther,
                                     surf.normal, normalOther))
            {
                continue;
            }
        }

        Reservoir spatial = emptyReservoir();
        // remove from here to separate 'init' pass
        {
            Surface surf_other = fetchGbufferSurface(pp);
            if (surf_other.isSky)
            {
                continue;
            }

            spatial = imageLoadReservoirInitial(pp);
        }


        float rnd = rnd16(seed, 105 + pixIndex);
        updateCombinedReservoir(
            combined, 
            spatial, rnd);
    }


    if (combined.weightSum <= 0.0 || combined.selected == LIGHT_INDEX_NONE)
    {
        return emptyReservoir();
    }

    return combined;
}


void processLight(uint seed, const Surface surf, int bounceIndex, inout LightResult out_result
#ifdef RAYGEN_COMMON_GRADIENTS
    , const Reservoir reservoir
#endif
)
{
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsLights;

    if ((globalUniform.lightCount == 0 && globalUniform.directionalLightExists == 0) || 
        (!castShadowRay && bounceIndex != 0) || 
        surf.sectorArrayIndex == SECTOR_INDEX_NONE)
    {
        return;
    }

    // note: if it's a gradient sample, then the seed is from previous frame

    const vec2 pointRnd = rndBlueNoise8(seed, RANDOM_SALT_LIGHT_POINT).xy * 0.99;

#if LIGHT_SAMPLE_METHOD == 0
  
    float rnd = rnd16(seed, RANDOM_SALT_LIGHT_CHOOSE(0));
    uint lt = globalUniform.lightCount;
    lt += globalUniform.directionalLightExists != 0 ? 1 : 0;
    uint selectedLightIndex = clamp(uint(rnd * lt), 0, lt - 1);
    float oneOverPdf = lt;

    LightSample light = sampleLight(lightSources[selectedLightIndex], surf.position, pointRnd);
    shade(surf, light, oneOverPdf, out_result.diffuse, out_result.specular);

#elif LIGHT_SAMPLE_METHOD == 2

#if !defined(RAYGEN_COMMON_GRADIENTS)
    // TODO: remove from here! for now, assume that processLight is called once per each pixel
    const ivec2 pix = ivec2(gl_LaunchIDEXT.xy);

    Reservoir reservoir = chooseLight(pix, seed, surf, pointRnd);
    imageStoreReservoir(reservoir, pix);
#endif

    shade(surf, reservoir, pointRnd, out_result.diffuse, out_result.specular);


    // TODO: remove, exists for compatibility
    uint selectedLightIndex; LightSample light;
    if (isReservoirValid(reservoir))
    {
        selectedLightIndex = reservoir.selected;
        light = sampleLight(lightSources[selectedLightIndex], surf.position, pointRnd);
    } 
#endif

#ifdef RAYGEN_ALLOW_FIREFLIES_CLAMP
    out_result.diffuse  = min(out_result.diffuse,  vec3(globalUniform.firefliesClamp));
    out_result.specular = min(out_result.specular, vec3(globalUniform.firefliesClamp));
#endif
    
    if (!castShadowRay || getLuminance(out_result.diffuse + out_result.specular) <= 0.0)
    {
        return;
    }
    
    out_result.shadowRayEnable = true;
    out_result.shadowRayEnd = light.position;
    out_result.shadowRayIgnoreFirstPersonViewer = (globalUniform.lightIndexIgnoreFPVShadows == selectedLightIndex);
}


void processDirectIllumination(uint seed, const Surface surf, int bounceIndex,
#ifdef RAYGEN_COMMON_DISTANCE_TO_LIGHT
    out float outDistance,
#endif
#ifdef RAYGEN_COMMON_GRADIENTS
    const Reservoir reservoir,
#endif
    out vec3 outDiffuse, out vec3 outSpecular)
{
    outDiffuse = outSpecular = vec3(0.0);
#ifdef RAYGEN_COMMON_DISTANCE_TO_LIGHT
    outDistance = MAX_RAY_LENGTH;
#endif

    LightResult r = newLightResult();
    processLight(
        seed,
        surf,
        bounceIndex, /* TODO: remove 'castShadowRay'? */
        r
#ifdef RAYGEN_COMMON_GRADIENTS
        , reservoir
#endif
    );

    bool isShadowed = false;
    if (r.shadowRayEnable)
    {
        const vec3 shadowRayStart = surf.position + surf.toViewerDir * RAY_ORIGIN_LEAK_BIAS;
        isShadowed = traceShadowRay(surf.instCustomIndex, shadowRayStart, r.shadowRayEnd, r.shadowRayIgnoreFirstPersonViewer);

#ifdef RAYGEN_COMMON_DISTANCE_TO_LIGHT
        outDistance = length(shadowRayStart - r.shadowRayEnd); 
#endif
    }

    outDiffuse  = r.diffuse  * float(!isShadowed);
    outSpecular = r.specular * float(!isShadowed);
}
#endif // RAYGEN_SHADOW_PAYLOAD

#endif // RAYGEN_COMMON_H_