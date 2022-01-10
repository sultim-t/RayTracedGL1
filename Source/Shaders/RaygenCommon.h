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


#include "Media.h"
#include "RayCone.h"

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

#include "RelieableMotionVectors.h"


layout(set = DESC_SET_TLAS, binding = BINDING_ACCELERATION_STRUCTURE_MAIN)   uniform accelerationStructureEXT topLevelAS;

#ifdef DESC_SET_CUBEMAPS
layout(set = DESC_SET_CUBEMAPS, binding = BINDING_CUBEMAPS) uniform samplerCube globalCubemaps[];
#endif
#ifdef DESC_SET_RENDER_CUBEMAP
layout(set = DESC_SET_RENDER_CUBEMAP, binding = BINDING_RENDER_CUBEMAP) uniform samplerCube renderCubemap;
#endif


layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload payload;

#ifdef RAYGEN_SHADOW_PAYLOAD
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayload payloadShadow;
#endif // RAYGEN_SHADOW_PAYLOAD



uint getPrimaryVisibilityCullMask()
{
    return globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT | INSTANCE_MASK_FIRST_PERSON;
}

uint getReflectionRefractionCullMask(uint surfInstCustomIndex, bool isRefraction)
{
    const uint world = globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT;

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
        globalUniform.rayCullMaskWorld | 
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



bool isPayloadConsistent(const ShPayload p)
{
    return p.instIdAndIndex != UINT32_MAX && p.geomAndPrimIndex != UINT32_MAX;
}

ShPayload newPayload()
{
    ShPayload r;
    r.baryCoords = vec2(0.0);
    r.instIdAndIndex = UINT32_MAX;
    r.geomAndPrimIndex = UINT32_MAX;

    return r;
}

ShPayload tracePrimaryRay(vec3 origin, vec3 direction)
{
    payload = newPayload();

    uint cullMask = getPrimaryVisibilityCullMask();

    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, globalUniform.primaryRayMinDist, direction, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT);

    return payload; 
}

ShPayload traceReflectionRefractionRay(vec3 origin, vec3 direction, uint surfInstCustomIndex, bool isRefraction, bool ignoreReflectRefractGeometry)
{
    payload = newPayload();

    uint cullMask = getReflectionRefractionCullMask(surfInstCustomIndex, isRefraction);

    if (ignoreReflectRefractGeometry)
    {
        cullMask = cullMask & (~INSTANCE_MASK_REFLECT_REFRACT);
    }
    
    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, 0.001, direction, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT);

    return payload; 
}

ShPayload traceIndirectRay(uint surfInstCustomIndex, vec3 surfPosition, vec3 bounceDirection)
{
    payload = newPayload();

    uint cullMask = getIndirectIlluminationCullMask(surfInstCustomIndex);

    traceRayEXT(
        topLevelAS,
        getAdditionalRayFlags(), 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        surfPosition, 0.001, bounceDirection, globalUniform.rayLength, 
        PAYLOAD_INDEX_DEFAULT); 

    return payload;
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
    uint    lightIndex;
    vec3    specular;
    bool    shadowRayEnable;
    vec3    shadowRayStart;
    bool    shadowRayIgnoreFirstPersonViewer;
    vec3    shadowRayEnd;
    uint    lightType;
};

LightResult newLightResult()
{
    LightResult r;
    r.diffuse           = vec3(0);
    r.lightIndex        = UINT32_MAX;
    r.specular          = vec3(0);
    r.shadowRayEnable   = false;
    r.shadowRayStart    = vec3(0);
    r.shadowRayEnd      = vec3(0);
    r.lightType         = LIGHT_TYPE_NONE;
    r.shadowRayIgnoreFirstPersonViewer = false;
    
    return r;
}



#define SHADOW_RAY_EPS 0.01

ShPayload traceShadowRay(uint surfInstCustomIndex, vec3 start, vec3 end, bool ignoreFirstPersonViewer /* = false */)
{
    payloadShadow = newPayload();

    uint cullMask = getShadowCullMask(surfInstCustomIndex);

    if (ignoreFirstPersonViewer)
    {
        cullMask &= ~INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }

    vec3 l = end - start;
    float maxDistance = length(l);
    l /= maxDistance;

    // removed gl_RayFlagsSkipClosestHitShaderEXT, as
    // occluder position is required for precise motion vectors
    traceRayEXT(
        topLevelAS, 
        getAdditionalRayFlags(),
        cullMask, 
        0, 0, 	// sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_SHADOW, 		// shadow missIndex
        start, 0.001, l, maxDistance - SHADOW_RAY_EPS, 
        PAYLOAD_INDEX_SHADOW);

    return payloadShadow;
}



#define MAX_SUBSET_LEN 8



vec3 getDirectionalLightVector(uint seed, const vec3 dirlightDirection, float dirlightTanAngularRadius)
{
    const vec2 u = getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(dirlightTanAngularRadius, u[0], u[1]);

    const mat3 basis = getONB(dirlightDirection);

    return normalize(dirlightDirection + basis[0] * disk.x + basis[1] * disk.y);
}


// resolvedSeedPrev must not be RESOLVED_SEED_INVALID
bool getDirectionalLightVector_Prev(out vec3 result, uint resolvedSeedPrev, const LightResult curDirLight, const vec3 surfPosition)
{
    if (globalUniform.lightCountDirectionalPrev == 0)
    {
        return false;
    }

    const vec3 l_prev = getDirectionalLightVector(resolvedSeedPrev, globalUniform.directionalLightDirectionPrev.xyz, globalUniform.directionalLightTanAngularRadius);
    
    result = surfPosition + l_prev * MAX_RAY_LENGTH;
    return true;
}


// toViewerDir -- is direction to viewer
void processDirectionalLight(
    uint seed, 
    uint surfInstCustomIndex, const vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir,
    bool isGradientSample,
    int bounceIndex,
    inout LightResult out_result)
{
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsDirectionalLights;

    if (globalUniform.lightCountDirectional == 0 || (!castShadowRay && bounceIndex != 0))
    {
        return;
    }

    const vec3 dirlightDirection            = globalUniform.directionalLightDirection.xyz;
    const vec3 dirlightColor                = globalUniform.directionalLightColor.xyz;
    const float dirlightTanAngularRadius    = globalUniform.directionalLightTanAngularRadius;

    float oneOverPdf = 1.0;

    const vec3 l = getDirectionalLightVector(seed, dirlightDirection, dirlightTanAngularRadius);

    const float nl = dot(surfNormal, l);
    const float ngl = dot(surfNormalGeom, l);

    if (nl <= 0 || ngl <= 0)
    {
        return;
    }

    out_result.lightIndex = 0;
    out_result.lightType = LIGHT_TYPE_DIRECTIONAL;

    out_result.diffuse  = evalBRDFLambertian(1.0) * dirlightColor * nl * M_PI;
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = evalBRDFSmithGGX(surfNormal, toViewerDir, dirlightDirection, surfRoughness, surfSpecularColor) * dirlightColor * nl;
#endif

    out_result.diffuse  *= oneOverPdf;
    out_result.specular *= oneOverPdf;

    if (!castShadowRay)
    {
        return;
    }

    out_result.shadowRayEnable   = true;
    out_result.shadowRayStart    = surfPosition;
    out_result.shadowRayEnd      = surfPosition + l * MAX_RAY_LENGTH;
}


// Ray Tracing Gems II. Chapter 20. Muliple Importance Sampling 101. 20.1.3 Light Sampling
float getGeometryFactor(const vec3 nSurf, const vec3 directionPtoPSurf, float distancePtoPSurf)
{
    float dist2 = distancePtoPSurf * distancePtoPSurf;
    return abs(-dot(nSurf, directionPtoPSurf)) / dist2;
}


float getGeometryFactorWoNormal(float distancePtoPSurf)
{
    float dist2 = distancePtoPSurf * distancePtoPSurf;
    return 1.0 / dist2;
}


vec3 getDirectionAndLength(const vec3 start, const vec3 end, out float outLength)
{
    vec3 atob = end - start;
    outLength = max(length(atob), 0.0001);
    
    return atob / outLength;
}


float getSphericalLightWeight(
    const vec3 surfPosition, const vec3 surfNormal, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir,
    uint plainLightListIndex)
{
    const uint sphLightIndex = plainLightList_Sph[plainLightListIndex];
    const ShLightSpherical light = lightSourcesSpherical[sphLightIndex];

    float dist;
    vec3 dirToCenter = getDirectionAndLength(surfPosition, light.position, dist);

    const float r = light.radius;
    const float z = light.falloff;
    
    const float I = pow(clamp((z - dist) / max(z - r, 1), 0, 1), 2);
    const vec3 c = I * light.color;

    const vec3 irradiance = M_PI * c * max(dot(surfNormal, dirToCenter), 0.0);
    const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

#ifdef RAYGEN_COMMON_ONLY_DIFFUSE
    return getLuminance(radiance);
#else

    const vec3 diff = radiance;
    const vec3 spec = 
        evalBRDFSmithGGX(surfNormal, toViewerDir, dirToCenter, surfRoughness, surfSpecularColor) * 
        light.color * 
        max(dot(surfNormal, dirToCenter), 0.0) * 
        getGeometryFactorWoNormal(dist);

    return getLuminance(diff + spec);
#endif
}


vec3 getSphericalLightPosition(uint seed, const vec3 dirToCenter, const vec3 sphLightCenter, float sphLightRadius, out vec3 lightNormal)
{
    // sample hemisphere visible to the surface point
    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_DISK).xy;

    float ltHsOneOverPdf;
    lightNormal = sampleOrientedHemisphere(-dirToCenter, u[0], u[1], ltHsOneOverPdf);

    //const float halfSphereArea = 2 * M_PI * sphLight.radius * sphLight.radius;
    //pdf /= max(halfSphereArea, 0.00001);

    return sphLightCenter + lightNormal * sphLightRadius;
}


// resolvedSeedPrev must not be RESOLVED_SEED_INVALID
// must be in sync with processSphericalLight
bool getSphericalLightPosition_Prev(out vec3 result, uint resolvedSeedPrev, const LightResult curSphLight, const vec3 surfPosition)
{
    uint sphLightIndexPrev = lightSourcesSphMatchPrev[curSphLight.lightIndex];

    if (sphLightIndexPrev == UINT32_MAX)
    {
        return false;
    }

    const ShLightSpherical sphLightPrev = lightSourcesSpherical_Prev[sphLightIndexPrev];

    float distToCenter;
    const vec3 dirToCenter = getDirectionAndLength(surfPosition, sphLightPrev.position, distToCenter);

    vec3 lightNormal;
    result = getSphericalLightPosition(resolvedSeedPrev, dirToCenter, sphLightPrev.position, sphLightPrev.radius, lightNormal);
    return true;
}


void processSphericalLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir, 
    bool isGradientSample,
    int bounceIndex,
    inout LightResult out_result)
{
    uint sphLightCount = isGradientSample ? globalUniform.lightCountSphericalPrev : globalUniform.lightCountSpherical;
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsSphereLights;

    if (sphLightCount == 0 || (!castShadowRay && bounceIndex != 0) || surfSectorArrayIndex == SECTOR_INDEX_NONE)
    {
        return;
    }

    // note: if it's a gradient sample, then the seed is from previous frame

    // random in [0,1)
    float rnd = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_CHOOSE).x * 0.99;

    const uint lightListBegin = sectorToLightListRegion_StartEnd_Sph[surfSectorArrayIndex * 2 + 0];
    const uint lightListEnd   = sectorToLightListRegion_StartEnd_Sph[surfSectorArrayIndex * 2 + 1];

    const uint S = uint(ceil(float(lightListEnd - lightListBegin) / MAX_SUBSET_LEN));
    const uint subsetStride = S;
    const uint subsetOffset = uint(floor(rnd * S));
    rnd = rnd * S - subsetOffset;

    uint  selected_plainLightListIndex = UINT32_MAX;
    float selected_mass = 0;

    float weightsTotal = 0;
    uint plainLightListIndex_iter = lightListBegin + subsetOffset;

    for (int i = 0; i < MAX_SUBSET_LEN; ++i) 
    {
        if (plainLightListIndex_iter >= lightListEnd) 
        {
            break;
        }

        const float w = getSphericalLightWeight(surfPosition, surfNormal, surfRoughness, surfSpecularColor, toViewerDir, 
                                                plainLightListIndex_iter);

        if (w > 0)
        {
            const float tau = weightsTotal / (weightsTotal + w);
            weightsTotal += w;

            if (rnd < tau)
            {
                rnd /= tau;
            }
            else
            {
                selected_plainLightListIndex = plainLightListIndex_iter;
                selected_mass = w;

                rnd = (rnd - tau) / (1 - tau);
            }

            rnd = clamp(rnd, 0, 0.999);
        }

        plainLightListIndex_iter += subsetStride;
    }

    if (weightsTotal <= 0.0 || selected_mass <= 0.0 || selected_plainLightListIndex == UINT32_MAX)
    {
        return;
    }

    float pdf = selected_mass / (weightsTotal * S);


    ShLightSpherical sphLight;
    uint sphLightIndex = plainLightList_Sph[selected_plainLightListIndex];

    if (!isGradientSample)
    {
        sphLight = lightSourcesSpherical[sphLightIndex];
    }
    else
    {        
        // the seed and other input params were replaced by prev frame's data,
        // so in some degree, lightIndex is the same as it was chosen in prev frame
        const uint prevFrameSphLightIndex = sphLightIndex;

        // get cur frame match for the chosen light
        sphLightIndex = lightSourcesSphMatchPrev[prevFrameSphLightIndex];

        // if light disappeared
        if (sphLightIndex == UINT32_MAX)
        {
            return;
        }

        sphLight = lightSourcesSpherical_Prev[sphLightIndex];
    }

    float distToCenter;
    const vec3 dirToCenter = getDirectionAndLength(surfPosition, sphLight.position, distToCenter);

    vec3 lightNormal;
    const vec3 posOnSphere = getSphericalLightPosition(seed, dirToCenter, sphLight.position, sphLight.radius, lightNormal);

    float distOntoSphere;
    const vec3 dirOntoSphere = getDirectionAndLength(surfPosition, posOnSphere, distOntoSphere);

    const float r = sphLight.radius;
    const float z = sphLight.falloff;
    
    const float I = pow(clamp((z - distToCenter) / max(z - r, 1), 0, 1), 2);
    float irradiance = I * max(dot(surfNormal, dirToCenter), 0.0);
    
#ifdef RAYGEN_ALLOW_FIREFLIES_CLAMP
    irradiance = min(irradiance, globalUniform.firefliesClamp);
#endif

    out_result.lightIndex = sphLightIndex;
    out_result.lightType = LIGHT_TYPE_SPHERICAL;

    out_result.diffuse = 
        evalBRDFLambertian(1.0) *
        sphLight.color * 
        M_PI * irradiance;
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = 
        evalBRDFSmithGGX(surfNormal, toViewerDir, dirOntoSphere, surfRoughness, surfSpecularColor) * 
        sphLight.color * 
        max(dot(surfNormal, dirOntoSphere), 0.0) *
        getGeometryFactor(lightNormal, dirOntoSphere, distOntoSphere);
#endif

    out_result.diffuse  /= pdf;
    out_result.specular /= pdf;
    
    if (!castShadowRay)
    {
        return;
    }
    
    out_result.shadowRayEnable = true;
    out_result.shadowRayStart  = surfPosition + toViewerDir * SHADOW_RAY_EPS;
    out_result.shadowRayEnd    = posOnSphere;
}


float getPolygonalLightWeight(const vec3 surfPosition, const vec3 surfNormalGeom, uint plainLightListIndex)
{
    const uint polyLightIndex = plainLightList_Poly[plainLightListIndex];
    const ShLightPolygonal polyLight = lightSourcesPolygonal[polyLightIndex];

    const vec3 triNormal = cross(polyLight.position_1.xyz - polyLight.position_0.xyz, polyLight.position_2.xyz - polyLight.position_0.xyz);
 
    const vec3 pointsOnUnitSphere[3] = 
    {
        normalize(polyLight.position_0.xyz - surfPosition),
        normalize(polyLight.position_1.xyz - surfPosition),
        normalize(polyLight.position_2.xyz - surfPosition),
    };

    if (-dot(pointsOnUnitSphere[0], triNormal) <= 0 && 
        -dot(pointsOnUnitSphere[1], triNormal) <= 0 && 
        -dot(pointsOnUnitSphere[2], triNormal) <= 0)
    {
        return 0;
    }
    
    if (dot(pointsOnUnitSphere[0], surfNormalGeom) <= 0 &&
        dot(pointsOnUnitSphere[1], surfNormalGeom) <= 0 &&
        dot(pointsOnUnitSphere[2], surfNormalGeom) <= 0)
    {
        return 0;
    }

    const float projTriArea = length(cross(pointsOnUnitSphere[1] - pointsOnUnitSphere[0], pointsOnUnitSphere[2] - pointsOnUnitSphere[0])) / 2.0;
    return getLuminance(polyLight.color) * projTriArea;
}


bool getPolygonalLightPosition_Prev(out vec3 result, uint resolvedSeedPrev, const LightResult curPolyLight)
{       
    uint polyLightIndexPrev = lightSourcesPolyMatchPrev[curPolyLight.lightIndex];

    if (polyLightIndexPrev == UINT32_MAX)
    {
        return false;
    }

    const ShLightPolygonal polyLightPrev = lightSourcesPolygonal_Prev[polyLightIndexPrev];

    const vec2 u = getRandomSample(resolvedSeedPrev, RANDOM_SALT_POLYGONAL_LIGHT_TRIANGLE_POINT).xy;    
    
    result = sampleTriangle(polyLightPrev.position_0.xyz, polyLightPrev.position_1.xyz, polyLightPrev.position_2.xyz, u[0], u[1]);
    return true;
}


void processPolygonalLight(
    uint seed, 
    uint surfInstCustomIndex, const vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir, 
    bool isGradientSample,
    int bounceIndex,
    inout LightResult out_result)
{
    uint polyLightCount = isGradientSample ? globalUniform.lightCountPolygonalPrev : globalUniform.lightCountPolygonal;
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsPolygonalLights;

    if (polyLightCount == 0 || (!castShadowRay && bounceIndex != 0) || surfSectorArrayIndex == SECTOR_INDEX_NONE)
    {
        return;
    }

    // using Subset Importance Sampling
    // Ray Tracing Gems II, chapter 47

    // random in [0,1)
    float rnd = getRandomSample(seed, RANDOM_SALT_POLYGONAL_LIGHT_CHOOSE).x * 0.99;

    const uint lightListBegin = sectorToLightListRegion_StartEnd_Poly[surfSectorArrayIndex * 2 + 0];
    const uint lightListEnd   = sectorToLightListRegion_StartEnd_Poly[surfSectorArrayIndex * 2 + 1];

    const uint S = uint(ceil(float(lightListEnd - lightListBegin) / MAX_SUBSET_LEN));
    const uint subsetStride = S;
    const uint subsetOffset = uint(floor(rnd * S));
    rnd = rnd * S - subsetOffset;

    uint  selected_plainLightListIndex = UINT32_MAX;
    float selected_mass = 0;

    float weightsTotal = 0;
    uint plainLightListIndex_iter = lightListBegin + subsetOffset;

    for (int i = 0; i < MAX_SUBSET_LEN; ++i) 
    {
        if (plainLightListIndex_iter >= lightListEnd) 
        {
            break;
        }

        const float w = getPolygonalLightWeight(surfPosition, surfNormalGeom, plainLightListIndex_iter);

        if (w > 0)
        {
            const float tau = weightsTotal / (weightsTotal + w);
            weightsTotal += w;

            if (rnd < tau)
            {
                rnd /= tau;
            }
            else
            {
                selected_plainLightListIndex = plainLightListIndex_iter;
                selected_mass = w;

                rnd = (rnd - tau) / (1 - tau);
            }

            rnd = clamp(rnd, 0, 0.999);
        }

        plainLightListIndex_iter += subsetStride;
    }

    if (weightsTotal <= 0.0 || selected_mass <= 0.0 || selected_plainLightListIndex == UINT32_MAX)
    {
        return;
    }

    float pdf = selected_mass / (weightsTotal * S);


    
    ShLightPolygonal polyLight;
    uint polyLightIndex = plainLightList_Poly[selected_plainLightListIndex];

    if (!isGradientSample)
    {
        polyLight = lightSourcesPolygonal[polyLightIndex];
    }
    else
    {
        // the seed and other input params were replaced by prev frame's data,
        // so in some degree, lightIndex is the same as it was chosen in prev frame
        const uint prevFrameLightIndex = polyLightIndex;

        // get cur frame match for the chosen light
        polyLightIndex = lightSourcesPolyMatchPrev[prevFrameLightIndex];

        // if light disappeared
        if (polyLightIndex == UINT32_MAX)
        {
            return;
        }

        polyLight = lightSourcesPolygonal_Prev[polyLightIndex];
    }


    vec3 triNormal = cross(polyLight.position_1.xyz - polyLight.position_0.xyz, polyLight.position_2.xyz - polyLight.position_0.xyz);
    const float triArea = length(triNormal) / 2.0;

    if (triArea < 0.0001)
    {
        return;
    }
    triNormal /= triArea * 2;
    pdf /= triArea;


    const vec2 u = getRandomSample(seed, RANDOM_SALT_POLYGONAL_LIGHT_TRIANGLE_POINT).xy;    
    const vec3 triPoint = sampleTriangle(polyLight.position_0.xyz, polyLight.position_1.xyz, polyLight.position_2.xyz, u[0], u[1]);

    
    float distToLightPoint;
    const vec3 l = getDirectionAndLength(surfPosition, triPoint, distToLightPoint);

    const float nl = dot(surfNormal, l);
    const float ngl = dot(surfNormalGeom, l);
    const float ll = -dot(triNormal, l);

    if (nl <= 0 || ngl <= 0 || ll <= 0)
    {
        return;
    }

    float irradiance = 
        pow(ll, globalUniform.polyLightSpotlightFactor) * 
        getGeometryFactor(triNormal, l, distToLightPoint);

#ifdef RAYGEN_ALLOW_FIREFLIES_CLAMP
    irradiance = min(irradiance, globalUniform.firefliesClamp);
#endif

    const vec3 s = polyLight.color * nl * irradiance;
    
    out_result.lightIndex = polyLightIndex;
    out_result.lightType = LIGHT_TYPE_POLYGONAL;

    out_result.diffuse  = evalBRDFLambertian(1.0) * M_PI * s;
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = evalBRDFSmithGGX(surfNormal, toViewerDir, l, surfRoughness, surfSpecularColor) * s;
#endif

    out_result.diffuse  /= pdf;
    out_result.specular /= pdf;

    if (!castShadowRay)
    {
        return;
    }

    out_result.shadowRayEnable = true;
    out_result.shadowRayStart  = surfPosition + toViewerDir * SHADOW_RAY_EPS;
    out_result.shadowRayEnd    = triPoint;
}


bool getSpotLightPosition_Prev(out vec3 result, uint resolvedSeedPrev, const LightResult curSpotLight)
{
    if (globalUniform.lightCountSpotlightPrev == 0)
    {
        return false;
    }

    const vec3 spotPos      = globalUniform.spotlightPositionPrev.xyz; 
    const vec3 spotDir      = globalUniform.spotlightDirectionPrev.xyz; 
    const vec3 spotUp       = globalUniform.spotlightUpVectorPrev.xyz; 
    // just use current
    const float spotRadius  = max(globalUniform.spotlightRadius, 0.001);

    const vec2 u = getRandomSample(resolvedSeedPrev, RANDOM_SALT_SPOT_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(spotRadius, u[0], u[1]);
    const vec3 spotRight = cross(spotDir, spotUp);
    
    result = spotPos + spotRight * disk.x + spotUp * disk.y;
    return true;
}


void processSpotLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir, 
    bool isGradientSample,
    int bounceIndex,
    inout LightResult out_result)
{
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsSpotlights;

    if (globalUniform.lightCountSpotlight == 0 || (!castShadowRay && bounceIndex != 0))
    {
        return;
    }

    const vec3 spotPos      = globalUniform.spotlightPosition.xyz; 
    const vec3 spotDir      = globalUniform.spotlightDirection.xyz; 
    const vec3 spotUp       = globalUniform.spotlightUpVector.xyz; 
    const vec3 spotColor    = globalUniform.spotlightColor.xyz;
    const float spotRadius  = max(globalUniform.spotlightRadius, 0.001);
    const float spotFalloff = globalUniform.spotlightFalloffDistance;
    const float spotCosAngleOuter = globalUniform.spotlightCosAngleOuter;
    const float spotCosAngleInner = globalUniform.spotlightCosAngleInner;

    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPOT_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(spotRadius, u[0], u[1]);
    const vec3 spotRight = cross(spotDir, spotUp);
    const vec3 posOnDisk = spotPos + spotRight * disk.x + spotUp * disk.y;

    const float oneOverPdf = 1.0 / (M_PI * spotRadius * spotRadius);

    const vec3 toLight = posOnDisk - surfPosition;
    const float dist = length(toLight);

    const vec3 dir = toLight / max(dist, 0.01);
    const float nl = dot(surfNormal, dir);
    const float ngl = dot(surfNormalGeom, dir);
    const float cosA = dot(-dir, spotDir);

    if (nl <= 0 || ngl <= 0 || cosA < spotCosAngleOuter)
    {
        return;
    }

    const float distWeight = pow(clamp((spotFalloff - dist) / max(spotFalloff, 1), 0, 1), 2);

    out_result.lightIndex = 0;
    out_result.lightType = LIGHT_TYPE_SPOTLIGHT;

    out_result.diffuse  = evalBRDFLambertian(1.0) * spotColor * distWeight * nl * M_PI;
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = evalBRDFSmithGGX(surfNormal, toViewerDir, dir, surfRoughness, surfSpecularColor) * spotColor * nl;
#endif

    const float angleWeight = square(smoothstep(spotCosAngleOuter, spotCosAngleInner, cosA));
    out_result.diffuse  *= angleWeight;
    out_result.specular *= angleWeight;

    // out_result.diffuse  *= oneOverPdf;
    // out_result.specular *= oneOverPdf;

    if (!castShadowRay)
    {
        return;
    }

    out_result.shadowRayEnable = true;
    out_result.shadowRayStart  = surfPosition;
    out_result.shadowRayEnd    = posOnDisk;
    out_result.shadowRayIgnoreFirstPersonViewer = true;
}


float getCandidateWeight(const LightResult c)
{
#ifdef RAYGEN_COMMON_ONLY_DIFFUSE
    return getLuminance(c.diffuse);
#else
    return getLuminance(c.diffuse + c.specular);
#endif
}


void processDirectIllumination(
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    ivec2 pix,
    out vec2 shadowMotionVector,
    out uint lightType,
    uint resolvedSeedPrev,
#endif
    uint seed, 
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir,
    bool isGradientSample,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    outDiffuse = outSpecular = vec3(0.0);


    LightResult selected = newLightResult();
    float selected_mass = 0.0;

    float rnd = getRandomSample(seed, RANDOM_SALT_LIGHT_TYPE_CHOOSE).x * 0.99;
    float weightsTotal = 0.0;


#define PROCESS_CANDIDATE(pfnProcessLight, pfnCandidateWeight)                  \
    {                                                                           \
        LightResult candidate = newLightResult();                               \
                                                                                \
        pfnProcessLight(                                                        \
            seed,                                                               \
            surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor, surfSectorArrayIndex,  \
            toViewerDir,                                                        \
            isGradientSample,                                                   \
            bounceIndex,                                                        \
            candidate);                                                         \
                                                                                \
        const float w = pfnCandidateWeight(candidate);                          \
                                                                                \
        if (w > 0)                                                              \
        {                                                                       \
            const float tau = weightsTotal / (weightsTotal + w);                \
            weightsTotal += w;                                                  \
                                                                                \
            if (rnd < tau)                                                      \
            {                                                                   \
                rnd /= tau;                                                     \
            }                                                                   \
            else                                                                \
            {                                                                   \
                selected = candidate;                                           \
                selected_mass = w;                                              \
                                                                                \
                rnd = (rnd - tau) / (1 - tau);                                  \
            }                                                                   \
                                                                                \
            rnd = clamp(rnd, 0, 0.999);                                         \
        }                                                                       \
    }


    PROCESS_CANDIDATE(processSphericalLight,    getCandidateWeight);
    PROCESS_CANDIDATE(processSpotLight,         getCandidateWeight);
    PROCESS_CANDIDATE(processPolygonalLight,    getCandidateWeight);
    PROCESS_CANDIDATE(processDirectionalLight,  getCandidateWeight);
    

    if (weightsTotal <= 0.0 || selected_mass <= 0.0)
    {
        return;
    }

    float pdf = selected_mass / weightsTotal;


    if (selected.shadowRayEnable)
    {
        const ShPayload blocker = traceShadowRay(surfInstCustomIndex, selected.shadowRayStart, selected.shadowRayEnd, selected.shadowRayIgnoreFirstPersonViewer);
        const bool isShadowed = isPayloadConsistent(blocker);

#ifndef RAYGEN_COMMON_ONLY_DIFFUSE

        shadowMotionVector = invalidateShadowMotionVector();
        lightType = LIGHT_TYPE_NONE;

        if (resolvedSeedPrev != RESOLVED_SEED_INVALID)
        {
            bool success = false;
            vec3 shadowRayEndPrev;

            switch (selected.lightType)
            {
                case LIGHT_TYPE_DIRECTIONAL: success = getDirectionalLightVector_Prev(shadowRayEndPrev, resolvedSeedPrev, selected, surfPosition); break;
                case LIGHT_TYPE_SPHERICAL:   success = getSphericalLightPosition_Prev(shadowRayEndPrev, resolvedSeedPrev, selected, surfPosition); break;
                case LIGHT_TYPE_POLYGONAL:   success = getPolygonalLightPosition_Prev(shadowRayEndPrev, resolvedSeedPrev, selected);               break;
                case LIGHT_TYPE_SPOTLIGHT:   success =      getSpotLightPosition_Prev(shadowRayEndPrev, resolvedSeedPrev, selected);               break;
            }

            if (success && setShadowMotionVector(shadowMotionVector, pix, blocker, selected.shadowRayEnd, shadowRayEndPrev))
            {
                lightType = selected.lightType;
            }
        }
#endif

        selected.diffuse  *= float(!isShadowed);
        selected.specular *= float(!isShadowed);
    }

    outDiffuse  += selected.diffuse  / pdf;
    outSpecular += selected.specular / pdf;
}
#endif // RAYGEN_SHADOW_PAYLOAD