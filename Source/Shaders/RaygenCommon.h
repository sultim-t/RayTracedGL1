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



#include "ShaderCommonGLSLFunc.h"



#if !defined(DESC_SET_TLAS) || \
    !defined(DESC_SET_GLOBAL_UNIFORM) || \
    !defined(DESC_SET_VERTEX_DATA) || \
    !defined(DESC_SET_TEXTURES) || \
    !defined(DESC_SET_RANDOM) || \
    !defined(DESC_SET_LIGHT_SOURCES)
        #error Descriptor set indices must be set!
#endif

#define LIGHT_SAMPLE_METHOD_NONE 0
#define LIGHT_SAMPLE_METHOD_DIRECT 1
#define LIGHT_SAMPLE_METHOD_INDIR 2
#define LIGHT_SAMPLE_METHOD_GRADIENTS 3
#define LIGHT_SAMPLE_METHOD_INITIAL 4
#if !defined(LIGHT_SAMPLE_METHOD)
    #error Light sampling method must be defined
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



layout(set = DESC_SET_TLAS, binding = BINDING_ACCELERATION_STRUCTURE_MAIN)   uniform accelerationStructureEXT topLevelAS;

#ifdef DESC_SET_CUBEMAPS
layout(set = DESC_SET_CUBEMAPS, binding = BINDING_CUBEMAPS) uniform samplerCube globalCubemaps[];
#endif

#ifdef DESC_SET_RENDER_CUBEMAP
layout(set = DESC_SET_RENDER_CUBEMAP, binding = BINDING_RENDER_CUBEMAP) uniform samplerCube renderCubemap;
#endif

#ifdef DESC_SET_PORTALS
layout(set = DESC_SET_PORTALS, binding = BINDING_PORTAL_INSTANCES) uniform readonly PortalInstances_BT
{
    ShPortalInstance g_portals[PORTAL_MAX_COUNT];
};
#endif


layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload g_payload;

#if LIGHT_SAMPLE_METHOD != LIGHT_SAMPLE_METHOD_NONE
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayloadShadow g_payloadShadow;
#endif



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



#if LIGHT_SAMPLE_METHOD != LIGHT_SAMPLE_METHOD_NONE

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

float traceVisibility(const Surface surf, const vec3 lightPosition, uint lightIndex)
{
    const vec3 start = surf.position + surf.toViewerDir * RAY_ORIGIN_LEAK_BIAS;
    const vec3 end = lightPosition;

    const bool ignoreFirstPersonViewer = (globalUniform.lightIndexIgnoreFPVShadows == lightIndex);

    const bool isShadowed = traceShadowRay(surf.instCustomIndex, start, end, ignoreFirstPersonViewer);
    return float(!isShadowed);
}
#endif // LIGHT_SAMPLE_METHOD != LIGHT_SAMPLE_METHOD_NONE



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
    specular = light.dw * nl * light.color * evalBRDFSmithGGX(surf.normal, surf.toViewerDir, l, surf.roughness, surf.specularColor);

    diffuse  *= oneOverPdf;
    specular *= oneOverPdf;
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

Reservoir calcInitialReservoir(uint seed, uint salt, const Surface surf, const vec2 pointRnd)
{
    #define INITIAL_SAMPLES 8
    
    Reservoir regularReservoir = emptyReservoir();
    if (isInsideCell(surf.position))
    {
        vec3 gridWorldPos = jitterPositionForLightGrid(surf.position, rnd8_4(seed, salt++).xyz);
        int lightGridBase = cellToArrayIndex(worldToCell(gridWorldPos));

        for (int i = 0; i < INITIAL_SAMPLES; i++)
        {
            // uniform distribution as a coarse source pdf
            float rnd = rnd16(seed, salt++);
            int lightGridArrayIndex = lightGridBase + 
                clamp(int(rnd * LIGHT_GRID_CELL_SIZE), 0, LIGHT_GRID_CELL_SIZE - 1);
            Reservoir r = unpackReservoirFromLightGrid(initialLightsGrid[lightGridArrayIndex]);

            if (!isReservoirValid(r))
            {
                continue;
            }

            uint xi = r.selected;
            float oneOverSourcePdf_xi = r.weightSum * safePositiveRcp(r.selected_targetPdf);

            LightSample lightSample = sampleLight(lightSources[xi], surf.position, pointRnd);
            float targetPdf_xi = targetPdfForLightSample(lightSample, surf);

            float rndRis = rnd16(seed, salt++);
            updateReservoir(regularReservoir, xi, targetPdf_xi, oneOverSourcePdf_xi, rndRis);
        }
    }
    else
    {      
        for (int i = 0; i < INITIAL_SAMPLES; i++)
        {
            // uniform distribution as a coarse source pdf
            float rnd = rnd16(seed, salt++);
            uint xi = LIGHT_ARRAY_REGULAR_LIGHTS_OFFSET + clamp(uint(rnd * globalUniform.lightCount), 0, globalUniform.lightCount - 1);
            float oneOverSourcePdf_xi = globalUniform.lightCount;

            LightSample lightSample = sampleLight(lightSources[xi], surf.position, pointRnd);
            float targetPdf_xi = targetPdfForLightSample(lightSample, surf);

            float rndRis = rnd16(seed, salt++);
            updateReservoir(regularReservoir, xi, targetPdf_xi, oneOverSourcePdf_xi, rndRis);
        }
    }
    normalizeReservoir(regularReservoir, 1);


    Reservoir dirLightReservoir = emptyReservoir();
    if (globalUniform.directionalLightExists != 0)
    {
        uint xi = LIGHT_ARRAY_DIRECTIONAL_LIGHT_OFFSET;
        float oneOverSourcePdf_xi = 1;

        LightSample lightSample = sampleLight(lightSources[xi], surf.position, pointRnd);
        float targetPdf_xi = targetPdfForLightSample(lightSample, surf);

        float rndRis = rnd16(seed, salt++);
        updateReservoir(dirLightReservoir, xi, targetPdf_xi, oneOverSourcePdf_xi, rndRis);
    }
    normalizeReservoir(dirLightReservoir, 1);

    #if LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_INITIAL
    if (isReservoirValid(dirLightReservoir))
    {
        LightSample lightSample = sampleLight(lightSources[dirLightReservoir.selected], surf.position, pointRnd);
        float v = traceVisibility(surf, lightSample.position, dirLightReservoir.selected);

        if (v < 0.5)
        {
            dirLightReservoir = emptyReservoir();
        }
    }
    #endif // LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_INITIAL

    float rnd = rnd16(seed, salt++); 
    Reservoir combined;
    initCombinedReservoir(
        combined, 
        regularReservoir);
    updateCombinedReservoir(
        combined, 
        dirLightReservoir, rnd);
    normalizeReservoir(combined, 1);

    return combined;
}

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

// Select light in screen-space for direct illumination
Reservoir selectLight_Direct(const ivec2 pix, uint seed, const Surface surf, const vec2 pointRnd)
{
    #define TEMPORAL_SAMPLES 1
    #define TEMPORAL_RADIUS 2
    #define SPATIAL_SAMPLES 8
    #define SPATIAL_RADIUS 30

    const ivec3 chRenderArea = getCheckerboardedRenderArea(pix); // assuming that pix is checkerboarded
    const float motionZ = texelFetch(framebufMotion_Sampler, pix, 0).z;
    const float depthCur = texelFetch(framebufDepthWorld_Sampler, pix, 0).r;
    const vec2 posPrev = getPrevScreenPos(framebufMotion_Sampler, pix);
    uint salt = RANDOM_SALT_LIGHT_CHOOSE_DIRECT_BASE;


    Reservoir initReservoir = imageLoadReservoirInitial(pix);
    
    Reservoir combined;
    initCombinedReservoir(
        combined, 
        initReservoir);


    // temporal
    for (int pixIndex = 0; pixIndex < TEMPORAL_SAMPLES; pixIndex++)
    {
        // TODO: need low discrepancy noise
        vec2 rndOffset = rnd8_4(seed, salt++).xy * 2.0 - 1.0;
        ivec2 pp = ivec2(floor(posPrev + rndOffset * TEMPORAL_RADIUS));

        {
            const float depthPrev = texelFetch(framebufDepthWorld_Prev_Sampler, pp, 0).r;
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

        float rnd = rnd16(seed, salt++);
        updateCombinedReservoir_newSurf(
            combined, 
            temporal, temporalTargetPdf_curSurf, rnd);
    } 

    for (int pixIndex = 0; pixIndex < SPATIAL_SAMPLES; pixIndex++)
    {
        // TODO: need low discrepancy noise
        vec2 rndOffset = rnd8_4(seed, salt++).xy * 2.0 - 1.0;
        ivec2 pp = pix + ivec2(rndOffset * SPATIAL_RADIUS);

        {
            const float depthOther = texelFetch(framebufDepthWorld_Sampler, pp, 0).r;
            const vec3 normalOther = texelFetchNormal(pp);

            if (!testSurfaceForReuse(chRenderArea, pp, 
                                     depthCur, depthOther,
                                     surf.normal, normalOther))
            {
                continue;
            }
        }

        Reservoir spatial = imageLoadReservoirInitial(pp);

        float rnd = rnd16(seed, salt++);
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

Reservoir selectLight_Uniform(uint seed)
{
    float rnd = rnd16(seed, RANDOM_SALT_LIGHT_CHOOSE_DIRECT_BASE);

    uint lt = globalUniform.lightCount;
    lt += globalUniform.directionalLightExists != 0 ? 1 : 0;

    Reservoir r = emptyReservoir();

    r.selected = clamp(uint(rnd * lt), 0, lt - 1);
    r.selected_targetPdf = 1.0 / float(lt);
    r.weightSum = 1.0;
    r.M = 1;

    return r;
}

// Select light in world-space for light bounces
Reservoir selectLight_Indir(uint seed, const Surface surf, const vec2 pointRnd)
{
    return calcInitialReservoir(seed, RANDOM_SALT_LIGHT_CHOOSE_INDIRECT_BASE, surf, pointRnd);
}

vec2 getLightPointRnd(uint seed)
{
    return rnd16_2(seed, RANDOM_SALT_LIGHT_POINT) * 0.99;
}

#if LIGHT_SAMPLE_METHOD != LIGHT_SAMPLE_METHOD_NONE
bool isDirectIlluminationValid(int bounceIndex)
{
    bool v = true;
    v = v && (bounceIndex < globalUniform.maxBounceShadowsLights || globalUniform.maxBounceShadowsLights == 0);
    v = v && (globalUniform.lightCount + globalUniform.directionalLightExists > 0);
    // v = v && (surf.sectorArrayIndex != SECTOR_INDEX_NONE);
    
    return v;
}

void traceDirectIllumination(const Surface surf, const Reservoir reservoir, const vec2 pointRnd, int bounceIndex, out float out_distance, out vec3 out_diffuse, out vec3 out_specular)
{    
    const LightSample light = sampleLight(lightSources[reservoir.selected], surf.position, pointRnd);
    shade(surf, light, calcSelectedSampleWeight(reservoir), out_diffuse, out_specular);
    
    if (getLuminance(out_diffuse + out_specular) <= 0.0)
    {
        out_diffuse = out_specular = vec3(0.0);
        return;
    }

    if (bounceIndex < globalUniform.maxBounceShadowsLights)
    {
        float visibility = traceVisibility(surf, light.position, reservoir.selected);

        out_diffuse  *= visibility;
        out_specular *= visibility;

    #if LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_DIRECT
        out_distance = length(light.position - surf.position);
    #endif
    }
}
#endif // LIGHT_SAMPLE_METHOD != LIGHT_SAMPLE_METHOD_NONE


#if LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_DIRECT
Reservoir processDirectIllumination(uint seed, const ivec2 pix, const Surface surf, out float out_distance, out vec3 out_diffuse, out vec3 out_specular)
{
    out_diffuse = out_specular = vec3(0.0);
    out_distance = MAX_RAY_LENGTH;

    if (!isDirectIlluminationValid(0))
    {
        return emptyReservoir();
    }
    const vec2 pointRnd = getLightPointRnd(seed);
    
    const Reservoir reservoir = selectLight_Direct(pix, seed, surf, pointRnd);
    if (!isReservoirValid(reservoir))
    {
        return emptyReservoir();
    } 

    traceDirectIllumination(surf, reservoir, pointRnd, 0, out_distance, out_diffuse, out_specular);
    return reservoir;
}
#endif

#if LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_INDIR
vec3 processDirectIllumination(uint seed, const Surface surf, int bounceIndex)
{
    if (!isDirectIlluminationValid(bounceIndex))
    {
        return vec3(0.0);
    }
    const vec2 pointRnd = getLightPointRnd(seed);
    
    const Reservoir reservoir = selectLight_Indir(seed, surf, pointRnd);
    if (!isReservoirValid(reservoir))
    {
        return vec3(0.0);
    } 

    vec3 out_diffuse;
    vec3 unusedv; float unusedf;
    traceDirectIllumination(surf, reservoir, pointRnd, bounceIndex, unusedf, out_diffuse, unusedv);
    
    return min(out_diffuse, vec3(globalUniform.firefliesClamp));
}
#endif

#if LIGHT_SAMPLE_METHOD == LIGHT_SAMPLE_METHOD_GRADIENTS
void processDirectIllumination(uint seed, const Surface surf, const Reservoir reservoir, out vec3 out_diffuse, out vec3 out_specular)
{
    out_diffuse = out_specular = vec3(0.0);

    if (!isDirectIlluminationValid(0))
    {
        return;
    }
    const vec2 pointRnd = getLightPointRnd(seed);

    if (!isReservoirValid(reservoir))
    {
        return;
    } 
    
    float unusedf;
    traceDirectIllumination(surf, reservoir, pointRnd, 0, unusedf, out_diffuse, out_specular);
}
#endif

#endif // RAYGEN_COMMON_H_