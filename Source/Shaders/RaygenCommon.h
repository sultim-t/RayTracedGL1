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


layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload payload;

#ifdef RAYGEN_SHADOW_PAYLOAD
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayloadShadow payloadShadow;
#endif // RAYGEN_SHADOW_PAYLOAD



uint getPrimaryVisibilityCullMask()
{
    return globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT | INSTANCE_MASK_FIRST_PERSON;
}

uint getReflectionRefractionCullMask(bool isRefraction)
{
    const uint world = globalUniform.rayCullMaskWorld | INSTANCE_MASK_REFLECT_REFRACT;
    
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
    
    // blended geometry doesn't have indirect illumination
}



uint getAdditionalRayFlags()
{
    return globalUniform.rayCullBackFaces != 0 ? gl_RayFlagsCullBackFacingTrianglesEXT : 0;
}



bool isPayloadConsistent(const ShPayload p)
{
    return p.instIdAndIndex != UINT32_MAX && p.geomAndPrimIndex != UINT32_MAX;
}

void resetPayload()
{
    payload.baryCoords = vec2(0.0);
    payload.instIdAndIndex = UINT32_MAX;
    payload.geomAndPrimIndex = UINT32_MAX;
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

    return payload; 
}

ShPayload traceReflectionRefractionRay(vec3 origin, vec3 direction, bool isRefraction, bool ignoreReflectRefractGeometry)
{
    resetPayload();

    uint cullMask = getReflectionRefractionCullMask(isRefraction) ;

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



#define SHADOW_RAY_EPS 0.01
#define SHADOW_CAST_LUMINANCE_THRESHOLD 0.000001

// l is pointed to the light
bool traceShadowRay(uint surfInstCustomIndex, vec3 start, vec3 end, bool ignoreFirstPersonViewer)
{
    // prepare shadow payload
    payloadShadow.isShadowed = 1;  

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

    return payloadShadow.isShadowed == 1;
}

bool traceShadowRay(uint surfInstCustomIndex, vec3 start, vec3 end)
{
    return traceShadowRay(surfInstCustomIndex, start, end, false);
}



// toViewerDir -- is direction to viewer
// distanceToViewer -- used for shadow ray origin fix, so it can't be under the surface
void processDirectionalLight(
    uint seed, 
    uint surfInstCustomIndex, const vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, float distanceToViewer,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsDirectionalLights;

    if (globalUniform.lightCountDirectional == 0 || (!castShadowRay && bounceIndex != 0))
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const vec3 dirlightDirection            = globalUniform.directionalLightDirection.xyz;
    const vec3 dirlightColor                = globalUniform.directionalLightColor.xyz;
    const float dirlightTanAngularRadius    = globalUniform.directionalLightTanAngularRadius;

    float oneOverPdf = 1.0;

    const vec2 u = getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(dirlightTanAngularRadius, u[0], u[1]);

    const mat3 basis = getONB(dirlightDirection);
    const vec3 l = normalize(dirlightDirection + basis[0] * disk.x + basis[1] * disk.y);

    const float nl = dot(surfNormal, l);
    const float ngl = dot(surfNormalGeom, l);

    if (nl <= 0 || ngl <= 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    outDiffuse = evalBRDFLambertian(1.0) * dirlightColor * nl * M_PI;
    outSpecular = evalBRDFSmithGGX(surfNormal, toViewerDir, dirlightDirection, surfRoughness, surfSpecularColor) * dirlightColor * nl;

    outDiffuse *= oneOverPdf;
    outSpecular *= oneOverPdf;

    // if too dim, don't cast shadow ray
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }

    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition, surfPosition + l * MAX_RAY_LENGTH);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
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


void processSphericalLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, 
    bool isGradientSample,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint sphLightCount = isGradientSample ? globalUniform.lightCountSphericalPrev : globalUniform.lightCountSpherical;
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsSphereLights;

    if (sphLightCount == 0 || (!castShadowRay && bounceIndex != 0))
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    // note: if it's a gradient sample, then the seed is from previous frame

    float weightSum = 0.0; 

    const int MAX_LIGHTS_PER_SAMPLE = 8;
    float weights[MAX_LIGHTS_PER_SAMPLE];
    vec4 rnds[MAX_LIGHTS_PER_SAMPLE / 4] = 
    {
        getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_INDEX(0)),
        getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_INDEX(1))
    };

    // choose light 
    for (int i = 0; i < MAX_LIGHTS_PER_SAMPLE; i++)
    {
        const float randomIndex = sphLightCount * rnds[i / 4][i % 4];
        const uint lightIndex = clamp(uint(randomIndex), 0, sphLightCount - 1);
        
        const ShLightSpherical light = lightSourcesSpherical[lightIndex];
        


        float dist;
        vec3 dir = getDirectionAndLength(surfPosition, light.position, dist);


        const float r = light.radius;
        const float z = light.falloff;

        if (dist < max(r, 0.001))
        {
            outDiffuse = light.color;
            outSpecular = light.color;
            return;
        }
        
        const float I = pow(clamp((z - dist) / max(z - r, 1), 0, 1), 2);
        const vec3 c = I * light.color;

        const vec3 irradiance = M_PI * c * max(dot(surfNormal, dir/*ToCenter*/), 0.0);
        const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

        const vec3 diff = radiance;
        const vec3 spec = 
            evalBRDFSmithGGX(surfNormal, toViewerDir, dir, surfRoughness, surfSpecularColor) * 
            light.color * 
            max(dot(surfNormal, dir), 0.0) * 
            getGeometryFactorWoNormal(dist);


        weights[i] = getLuminance(diff + spec);
        weightSum += weights[i];
    }

    if (weightSum <= 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }


    // choose a light source with its appropriate probability
    uint sphLightIndex = UINT32_MAX;
    float rand = weightSum * getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_CHOOSE).x;
    float pdf = 0;
    
    for (int i = 0; i < MAX_LIGHTS_PER_SAMPLE; i++)
    {
        pdf = weights[i];
        rand -= pdf;

        const float fIndex  = sphLightCount * rnds[i / 4][i % 4];
        sphLightIndex = clamp(uint(fIndex), 0, sphLightCount - 1);

        if (rand <= 0)
        {
            break;
        }
    }
    
    if (rand > 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    pdf = max(pdf / weightSum / sphLightCount, 0.001);


    if (isGradientSample)
    {        
        // choose light using prev frame's info
        const uint prevFrameSphLightIndex = sphLightIndex;

        // get cur frame match for the chosen light
        sphLightIndex = lightSourcesSphMatchPrev[prevFrameSphLightIndex];

        // if light disappeared
        if (sphLightIndex == UINT32_MAX)
        {
            outDiffuse = vec3(0.0);
            outSpecular = vec3(0.0);
            return;
        }
    }

    const ShLightSpherical sphLight = lightSourcesSpherical[sphLightIndex];

    float distToCenter;
    const vec3 dirToCenter = getDirectionAndLength(surfPosition, sphLight.position, distToCenter);

    // sample hemisphere visible to the surface point
    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_DISK).xy;
    float ltHsOneOverPdf;
    const vec3 lightNormal = sampleOrientedHemisphere(-dirToCenter, u[0], u[1], ltHsOneOverPdf);
    const float halfSphereArea = 2 * M_PI * sphLight.radius * sphLight.radius;
    //pdf /= max(halfSphereArea, 0.00001);
    
    const vec3 posOnSphere = sphLight.position + lightNormal * sphLight.radius;

    float distOntoSphere;
    const vec3 dirOntoSphere = getDirectionAndLength(surfPosition, posOnSphere, distOntoSphere);

    const float r = sphLight.radius;
    const float z = sphLight.falloff;
    
    const float i = pow(clamp((z - distToCenter) / max(z - r, 1), 0, 1), 2);
    const vec3 c = i * sphLight.color;

    const vec3 irradiance = M_PI * c * max(dot(surfNormal, dirToCenter), 0.0);
    const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

    outDiffuse = radiance;
    outSpecular = 
        evalBRDFSmithGGX(surfNormal, toViewerDir, dirOntoSphere, surfRoughness, surfSpecularColor) * 
        sphLight.color * 
        max(dot(surfNormal, dirOntoSphere), 0.0) *
        getGeometryFactor(lightNormal, dirOntoSphere, distOntoSphere);

    outDiffuse /= pdf;
    outSpecular /= pdf;
    
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }
    
    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition + toViewerDir * SHADOW_RAY_EPS, posOnSphere);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
}


void processPolygonalLight(
    uint seed, 
    uint surfInstCustomIndex, const vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, 
    bool isGradientSample,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint polyLightCount = isGradientSample ? globalUniform.lightCountPolygonalPrev : globalUniform.lightCountPolygonal;
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsPolygonalLights;

    if (polyLightCount == 0 || (!castShadowRay && bounceIndex != 0))
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    // if it's a gradient sample, then the seed is from previous frame
    const float randomIndex = polyLightCount * getRandomSample(seed, RANDOM_SALT_POLYGONAL_LIGHT_CHOOSE).x;
    uint polyLightIndex = clamp(uint(randomIndex), 0, polyLightCount - 1);;

    if (isGradientSample)
    {
        // choose light using prev frame's info
        const uint prevFramePolyLightIndex = polyLightIndex;

        // get cur frame match for the chosen light
        polyLightIndex = lightSourcesPolyMatchPrev[prevFramePolyLightIndex];

        // if light disappeared
        if (polyLightIndex == UINT32_MAX)
        {
            outDiffuse = vec3(0.0);
            outSpecular = vec3(0.0);
            return;
        }
    }

    float oneOverPdf = polyLightCount;
    const ShLightPolygonal polyLight = lightSourcesPolygonal[polyLightIndex];


    vec3 triNormal = cross(polyLight.position_1 - polyLight.position_0, polyLight.position_2 - polyLight.position_0);
    const float triArea = length(triNormal) / 2.0;

    if (triArea < 0.0001)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }
    triNormal /= triArea * 2;
    oneOverPdf *= triArea;


    const vec2 u = getRandomSample(seed, RANDOM_SALT_POLYGONAL_LIGHT_TRIANGLE_POINT).xy;    
    const vec3 triPoint = sampleTriangle(polyLight.position_0, polyLight.position_1, polyLight.position_2, u[0], u[1]);

    
    float distToLightPoint;
    const vec3 l = getDirectionAndLength(surfPosition, triPoint, distToLightPoint);

    const float nl = dot(surfNormal, l);
    const float ngl = dot(surfNormalGeom, l);

    if (nl <= 0 || ngl <= 0 || dot(triNormal, l) <= 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const vec3 polyLightColor = vec3(polyLight.color_R, polyLight.color_G, polyLight.color_B);
    
    const vec3 s =
        polyLightColor * 
        nl * 
        getGeometryFactor(triNormal, l, distToLightPoint);

    outDiffuse  = evalBRDFLambertian(1.0) * M_PI * s;
    outSpecular = evalBRDFSmithGGX(surfNormal, toViewerDir, l, surfRoughness, surfSpecularColor) * s;

    outDiffuse *= oneOverPdf;
    outSpecular *= oneOverPdf;

    // if too dim, don't cast shadow ray
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }

    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition + toViewerDir * SHADOW_RAY_EPS, triPoint);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
}


void processSpotLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, 
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    bool castShadowRay = bounceIndex < globalUniform.maxBounceShadowsSpotlights;

    if (globalUniform.lightCountSpotlight == 0 || (!castShadowRay && bounceIndex != 0))
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const vec3 spotPos = globalUniform.spotlightPosition.xyz; 
    const vec3 spotDir = globalUniform.spotlightDirection.xyz; 
    const vec3 spotUp = globalUniform.spotlightUpVector.xyz; 
    const vec3 spotColor = globalUniform.spotlightColor.xyz;
    const float spotRadius = max(globalUniform.spotlightRadius, 0.001);
    const float spotCosAngleOuter = globalUniform.spotlightCosAngleOuter;
    const float spotCosAngleInner = globalUniform.spotlightCosAngleInner;
    const float spotFalloff = globalUniform.spotlightFalloffDistance;

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
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const float distWeight = pow(clamp((spotFalloff - dist) / max(spotFalloff, 1), 0, 1), 2);

    outDiffuse = evalBRDFLambertian(1.0) * spotColor * distWeight * nl * M_PI;
    outSpecular = evalBRDFSmithGGX(surfNormal, toViewerDir, dir, surfRoughness, surfSpecularColor) * spotColor * nl;

    const float angleWeight = square(smoothstep(spotCosAngleOuter, spotCosAngleInner, cosA));
    outDiffuse *= angleWeight;
    outSpecular *= angleWeight;

    // outDiffuse *= oneOverPdf;
    // outSpecular *= oneOverPdf;

    // if too dim, don't cast shadow ray
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }

    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition, posOnDisk, true);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
}


void processDirectIllumination(
    uint seed, 
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, float distanceToViewer,
    bool isGradientSample,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    // always cast shadow ray for directional lights
    vec3 dirDiff, dirSpec;
    processDirectionalLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, distanceToViewer,
        bounceIndex,
        dirDiff, dirSpec);
    
    vec3 sphDiff, sphSpec;
    processSphericalLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, 
        isGradientSample,  
        bounceIndex,
        sphDiff, sphSpec);

    vec3 spotDiff, spotSpec;
    processSpotLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, 
        bounceIndex,
        spotDiff, spotSpec);

    vec3 polyDiff, polySpec;
    processPolygonalLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, 
        isGradientSample,  
        bounceIndex,
        polyDiff, polySpec);
    
    outDiffuse = dirDiff + sphDiff + spotDiff + polyDiff;
    outSpecular = dirSpec + sphSpec + spotSpec + polySpec;
}


void processDirectIllumination(
    uint seed, 
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir,
    bool isGradientSample,
    int bounceIndex,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    // TODO: length(..) if reflect/refract
    processDirectIllumination(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, length(surfPosition - globalUniform.cameraPosition.xyz),
        isGradientSample,
        bounceIndex,
        outDiffuse, outSpecular);
}
#endif // RAYGEN_SHADOW_PAYLOAD