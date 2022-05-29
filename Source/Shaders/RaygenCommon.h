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



#define MAX_SUBSET_LEN 8



vec3 getDirectionalLightVector(uint seed, const vec3 dirlightDirection, float dirlightTanAngularRadius)
{
    const vec2 u = getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(dirlightTanAngularRadius, u[0], u[1]);

    const mat3 basis = getONB(dirlightDirection);

    return normalize(dirlightDirection + basis[0] * disk.x + basis[1] * disk.y);
}



#define MAX_SUBSET_LEN 8


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
    out_result.shadowRayStart    = surfPosition + toViewerDir * RAY_ORIGIN_LEAK_BIAS;
    out_result.shadowRayEnd      = surfPosition + l * MAX_RAY_LENGTH;
}


// Ray Tracing Gems II. Chapter 20. Muliple Importance Sampling 101. 20.1.3 Light Sampling
float getGeometryFactor(const vec3 nSurf, const vec3 directionPtoPSurf, float distancePtoPSurf)
{
    float dist2 = distancePtoPSurf * distancePtoPSurf;
    dist2 = max(dist2, 1.0);
    return abs(-dot(nSurf, directionPtoPSurf)) / dist2;
}


struct DirectionAndLength { vec3 dir; float len; };

DirectionAndLength calcDirectionAndLength(const vec3 start, const vec3 end)
{
    DirectionAndLength r;
    r.dir = end - start;
    r.len = max(length(r.dir), 0.0001);
    r.dir /= r.len;

    return r;
}


float getSphericalLightWeight(
    const vec3 surfPosition, const vec3 surfNormal, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir,
    uint plainLightListIndex)
{
    const uint sphLightIndex = plainLightList_Sph[plainLightListIndex];
    const ShLightSpherical sphLight = lightSourcesSpherical[sphLightIndex];

    float r = max(sphLight.radius, 1.0); // TODO: should be used sphLight.radius, but there are artifacts, like if some samples are too small or large
    float dist = length(sphLight.position - surfPosition);

    // solid angle here is the spherical cap area on a unit sphere
    float sinTheta = r / dist;
    float cosTheta = sqrt(1.0 - sinTheta * sinTheta);
    float solidAngle = 2 * M_PI * (1.0 - cosTheta);

    return solidAngle * getLuminance(sphLight.color);
}


struct PointOnSphericalLight { vec3 position; vec3 normal; };

PointOnSphericalLight getPointOnSphericalLight(uint seed, const ShLightSpherical sphLight, const vec3 surfPosition)
{
    PointOnSphericalLight r;
    const vec3 dirToCenter = safeNormalize(sphLight.position - surfPosition);

    // sample hemisphere visible to the surface point
    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_DISK).xy;
    float ltHsOneOverPdf;
    r.normal = sampleOrientedHemisphere(-dirToCenter, u[0], u[1], ltHsOneOverPdf);

    //const float halfSphereArea = 2 * M_PI * sphLight.radius * sphLight.radius;
    //pdf /= max(halfSphereArea, 0.00001);

    r.position = sphLight.position + r.normal * sphLight.radius;

    return r;
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

    const PointOnSphericalLight pointOnLight = getPointOnSphericalLight(seed, sphLight, surfPosition);
    const DirectionAndLength toLight = calcDirectionAndLength(surfPosition, pointOnLight.position);

    float nl = max(dot(surfNormal, toLight.dir), 0.0);

    vec3 irradiance = sphLight.color * getGeometryFactor(pointOnLight.normal, toLight.dir, toLight.len);

    out_result.lightIndex = sphLightIndex;
    out_result.lightType = LIGHT_TYPE_SPHERICAL;

    out_result.diffuse  = nl * irradiance * evalBRDFLambertian(1.0);
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = nl * irradiance * evalBRDFSmithGGX(surfNormal, toViewerDir, toLight.dir, surfRoughness, surfSpecularColor);
#endif

    out_result.diffuse  /= pdf;
    out_result.specular /= pdf;
    
    if (!castShadowRay)
    {
        return;
    }
    
    out_result.shadowRayEnable = true;
    out_result.shadowRayStart  = surfPosition + toViewerDir * RAY_ORIGIN_LEAK_BIAS;
    out_result.shadowRayEnd    = pointOnLight.position;
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

    
    const DirectionAndLength toLight = calcDirectionAndLength(surfPosition, triPoint);

    const float nl = dot(surfNormal, toLight.dir);
    const float ngl = dot(surfNormalGeom, toLight.dir);
    const float ll = -dot(triNormal, toLight.dir);

    if (nl <= 0 || ngl <= 0 || ll <= 0)
    {
        return;
    }

    vec3 irradiance = polyLight.color * getGeometryFactor(triNormal, toLight.dir, toLight.len);
    irradiance *= pow(ll, globalUniform.polyLightSpotlightFactor);
    
    out_result.lightIndex = polyLightIndex;
    out_result.lightType = LIGHT_TYPE_POLYGONAL;

    out_result.diffuse  = nl * irradiance * evalBRDFLambertian(1.0);
#ifndef RAYGEN_COMMON_ONLY_DIFFUSE
    out_result.specular = nl * irradiance * evalBRDFSmithGGX(surfNormal, toViewerDir, toLight.dir, surfRoughness, surfSpecularColor);
#endif

    out_result.diffuse  /= pdf;
    out_result.specular /= pdf;

    if (!castShadowRay)
    {
        return;
    }

    out_result.shadowRayEnable = true;
    out_result.shadowRayStart  = surfPosition + toViewerDir * RAY_ORIGIN_LEAK_BIAS;
    out_result.shadowRayEnd    = triPoint;
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
    uint seed, 
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor, uint surfSectorArrayIndex,
    const vec3 toViewerDir,
    bool isGradientSample,
    int bounceIndex,
#ifdef RAYGEN_COMMON_DISTANCE_TO_LIGHT
    out float outDistance,
#endif
    out vec3 outDiffuse, out vec3 outSpecular)
{
    outDiffuse = outSpecular = vec3(0.0);

#ifdef RAYGEN_COMMON_DISTANCE_TO_LIGHT
    outDistance = MAX_RAY_LENGTH;
    #define APPEND_DIST(x) outDistance = min(outDistance, length(x)) 
#else
    #define APPEND_DIST(x)
#endif


#define SEPARATE_SHADOW_RAYS
#ifdef SEPARATE_SHADOW_RAYS
    
    LightResult selected;

#define PROCESS_SEPARATELY(pfnProcessLight)                                     \
    {                                                                           \
        selected = newLightResult();                                            \
                                                                                \
        pfnProcessLight(                                                        \
            seed,                                                               \
            surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor, surfSectorArrayIndex,  \
            toViewerDir,                                                        \
            isGradientSample,                                                   \
            bounceIndex,                                                        \
            selected);                                                          \
                                                                                \
        bool isShadowed = false;                                                \
                                                                                \
        if (selected.shadowRayEnable)                                           \
        {                                                                       \
            isShadowed = traceShadowRay(surfInstCustomIndex, selected.shadowRayStart, selected.shadowRayEnd, selected.shadowRayIgnoreFirstPersonViewer);    \
            APPEND_DIST(selected.shadowRayStart - selected.shadowRayEnd);       \
        }                                                                       \
                                                                                \
        outDiffuse  += selected.diffuse  * float(!isShadowed);                  \
        outSpecular += selected.specular * float(!isShadowed);                  \
    }

    PROCESS_SEPARATELY(processDirectionalLight);
    PROCESS_SEPARATELY(processSphericalLight);
    PROCESS_SEPARATELY(processPolygonalLight);
    PROCESS_SEPARATELY(processSpotLight);
    

#else // !SEPARATE_SHADOW_RAYS


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
        bool isShadowed = traceShadowRay(surfInstCustomIndex, selected.shadowRayStart, selected.shadowRayEnd, selected.shadowRayIgnoreFirstPersonViewer);

        selected.diffuse  *= float(!isShadowed);
        selected.specular *= float(!isShadowed);
    }

    outDiffuse  += selected.diffuse  / pdf;
    outSpecular += selected.specular / pdf;

#endif // SEPARATE_SHADOW_RAYS

}
#endif // RAYGEN_SHADOW_PAYLOAD