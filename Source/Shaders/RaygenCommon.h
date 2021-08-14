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


#include "ShaderCommonGLSLFunc.h"


#if !defined(DESC_SET_TLAS) || \
    !defined(DESC_SET_GLOBAL_UNIFORM) || \
    !defined(DESC_SET_VERTEX_DATA) || \
    !defined(DESC_SET_TEXTURES) || \
    !defined(DESC_SET_RANDOM) || \
    !defined(DESC_SET_LIGHT_SOURCES)
        #error Descriptor set indices must be set!
#endif


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
    // without reflective/refractive
    const uint world = globalUniform.rayCullMaskWorld & (~INSTANCE_MASK_REFLECT_REFRACT);
    
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
    
    // blended geometry doesn't have shadows
}

uint getIndirectIlluminationCullMask(uint surfInstCustomIndex)
{
    const uint world = globalUniform.rayCullMaskWorld & (~INSTANCE_MASK_REFLECT_REFRACT);
    
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
        gl_RayFlagsNoneEXT, 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, 0.001, direction, MAX_RAY_LENGTH, 
        PAYLOAD_INDEX_DEFAULT);

    return payload; 
}

ShPayload traceReflectionRefractionRay(vec3 origin, vec3 direction, bool isRefraction)
{
    resetPayload();

    uint cullMask = getReflectionRefractionCullMask(isRefraction);

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsNoneEXT, 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, 0.001, direction, MAX_RAY_LENGTH, 
        PAYLOAD_INDEX_DEFAULT);

    return payload; 
}

ShPayload traceIndirectRay(uint surfInstCustomIndex, vec3 surfPosition, vec3 bounceDirection)
{
    resetPayload();

    uint cullMask = getIndirectIlluminationCullMask(surfInstCustomIndex);

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsNoneEXT, 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        surfPosition, 0.001, bounceDirection, MAX_RAY_LENGTH, 
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

// l is pointed to the light
bool traceShadowRay(uint surfInstCustomIndex, vec3 o, vec3 l, float maxDistance)
{
    // prepare shadow payload
    payloadShadow.isShadowed = 1;  

    uint cullMask = getShadowCullMask(surfInstCustomIndex);

    traceRayEXT(
        topLevelAS, 
        gl_RayFlagsSkipClosestHitShaderEXT, 
        cullMask, 
        0, 0, 	// sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_SHADOW, 		// shadow missIndex
        o, 0.001, l, maxDistance, 
        PAYLOAD_INDEX_SHADOW);

    return payloadShadow.isShadowed == 1;
}

bool traceShadowRay(uint surfInstCustomIndex, vec3 o, vec3 l)
{
    return traceShadowRay(surfInstCustomIndex, o, l, MAX_RAY_LENGTH);
}



#define SHADOW_RAY_EPS_MIN      0.001
#define SHADOW_RAY_EPS_MAX      0.1
#define SHADOW_RAY_EPS_MAX_DIST 25

#define SHADOW_CAST_LUMINANCE_THRESHOLD 0.000001



#define AIR_TRANSMITTANCE_SIGMA 0.01

float getAirTransmittance(const float distante)
{
    // very coarse air transmittance
    return exp(-distante * AIR_TRANSMITTANCE_SIGMA);
}



// toViewerDir -- is direction to viewer
// distanceToViewer -- used for shadow ray origin fix, so it can't be under the surface
void processDirectionalLight(
    uint seed, 
    uint surfInstCustomIndex, const vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, float distanceToViewer,
    bool isGradientSample,
    bool castShadowRay,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint dirLightCount = isGradientSample ? globalUniform.lightCountDirectionalPrev : globalUniform.lightCountDirectional;

    if (dirLightCount == 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    // if it's a gradient sample, then the seed is from previous frame
    const float randomIndex = dirLightCount * getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_INDEX).x;
    uint dirLightIndex = clamp(uint(randomIndex), 0, dirLightCount - 1);;

    if (isGradientSample)
    {
        // choose light using prev frame's info
        const uint prevFrameDirLightIndex = dirLightIndex;

        // get cur frame match for the chosen light
        dirLightIndex = lightSourcesDirMatchPrev[prevFrameDirLightIndex];

        // if light disappeared
        if (dirLightIndex == UINT32_MAX)
        {
            outDiffuse = vec3(0.0);
            outSpecular = vec3(0.0);
            return;
        }
    }

    const float oneOverPdf = dirLightCount;

    const ShLightDirectional dirLight = lightSourcesDirecitional[dirLightIndex];

    const vec2 u = getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_DISK).xy;    
    const vec2 disk = sampleDisk(dirLight.tanAngularRadius, u[0], u[1]);

    const mat3 basis = getONB(dirLight.direction);
    const vec3 dir = normalize(dirLight.direction + basis[0] * disk.x + basis[1] * disk.y);

    const float nl = dot(surfNormal, dir);
    const float ngl = dot(surfNormalGeom, dir);

    if (nl <= 0 || ngl <= 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    outDiffuse = evalBRDFLambertian(1.0) * dirLight.color * nl * M_PI;
    outSpecular = evalBRDFSmithGGX(surfNormal, toViewerDir, dirLight.direction, surfRoughness, surfSpecularColor) * dirLight.color * nl;

    outDiffuse *= oneOverPdf;
    outSpecular *= oneOverPdf;

    // if too dim, don't cast shadow ray
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }

    const float shadowRayEps = mix(SHADOW_RAY_EPS_MIN, SHADOW_RAY_EPS_MAX, distanceToViewer / SHADOW_RAY_EPS_MAX_DIST);
    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition, dir);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
}

void processSphericalLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, 
    bool isGradientSample,
    bool castShadowRay,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint sphLightCount = isGradientSample ? globalUniform.lightCountSphericalPrev : globalUniform.lightCountSpherical;

    if (sphLightCount == 0)
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
        


        vec3 dir = light.position - surfPosition;

        const float r = light.radius;
        const float z = light.falloff;
        const float d = length(dir);

        if (d < max(r, 0.001))
        {
            outDiffuse = light.color;
            outSpecular = light.color;
            return;
        }

        dir /= max(d, 0.001);
        
        const float I = pow(clamp((z - d) / max(z - r, 1), 0, 1), 2);
        const vec3 c = I * light.color;

        const vec3 irradiance = M_PI * c * max(dot(surfNormal, dir/*ToCenter*/), 0.0);
        const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

        const vec3 diff = radiance;
        const vec3 spec = 
            evalBRDFSmithGGX(surfNormal, toViewerDir, dir, surfRoughness, surfSpecularColor) * 
            light.color * 
            dot(surfNormal, dir) * 
            getAirTransmittance(d);


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

    pdf = max(pdf / weightSum, 0.001);


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

    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_DISK).xy;
    const vec3 posOnSphere = sphLight.position + sampleSphere(u[0], u[1]) * sphLight.radius;

    vec3 dir = posOnSphere - surfPosition;
    const float distance = length(dir);

    surfPosition += (toViewerDir + surfNormalGeom) * SHADOW_RAY_EPS_MIN;

    // processed previously
    /*if (distance <= max(sphLight.radius, 0.001))
    {
        outDiffuse = sphLight.color * oneOverPdf;
        outSpecular = sphLight.color * oneOverPdf;
        return;
    }*/
    
    dir = dir / distance;

    vec3 dirToCenter = sphLight.position - surfPosition;

    const float r = sphLight.radius;
    const float z = sphLight.falloff;
    const float d = max(length(dirToCenter), 0.0001);
    dirToCenter /= d;
    
    const float i = pow(clamp((z - d) / max(z - r, 1), 0, 1), 2);
    const vec3 c = i * sphLight.color;

    const vec3 irradiance = M_PI * c * max(dot(surfNormal, dirToCenter), 0.0);
    const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

    outDiffuse = radiance;
    outSpecular = 
        evalBRDFSmithGGX(surfNormal, toViewerDir, dir, surfRoughness, surfSpecularColor) * 
        sphLight.color * 
        dot(surfNormal, dir) *
        getAirTransmittance(d);

    outDiffuse /= pdf;
    outSpecular /= pdf;
    
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }
    
    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition, dir, distance);

    outDiffuse *= float(!isShadowed);
    outSpecular *= float(!isShadowed);
}

void processSpotLight(
    uint seed,
    uint surfInstCustomIndex, vec3 surfPosition, const vec3 surfNormal, const vec3 surfNormalGeom, float surfRoughness, const vec3 surfSpecularColor,
    const vec3 toViewerDir, 
    bool isGradientSample,
    bool castShadowRay,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    if (globalUniform.spotlightCosAngleOuter <= 0.0 || 
        globalUniform.spotlightRadius <= 0.0 ||
        globalUniform.spotlightFalloffDistance <= 0.0)
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
    
    outSpecular *= getAirTransmittance(dist);

    // outDiffuse *= oneOverPdf;
    // outSpecular *= oneOverPdf;

    // if too dim, don't cast shadow ray
    if (!castShadowRay || getLuminance(outDiffuse) + getLuminance(outSpecular) < SHADOW_CAST_LUMINANCE_THRESHOLD)
    {
        return;
    }

    const bool isShadowed = traceShadowRay(surfInstCustomIndex, surfPosition, dir, dist);

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
        isGradientSample, 
        bounceIndex < globalUniform.maxBounceShadowsDirectionalLights,
        dirDiff, dirSpec);
    
    vec3 sphDiff, sphSpec;
    processSphericalLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, 
        isGradientSample,  
        bounceIndex < globalUniform.maxBounceShadowsSphereLights,
        sphDiff, sphSpec);

    vec3 spotDiff, spotSpec;
    processSpotLight(
        seed, 
        surfInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, surfSpecularColor,
        toViewerDir, 
        isGradientSample,  
        bounceIndex < globalUniform.maxBounceShadowsSpotlights,
        spotDiff, spotSpec);
    
    outDiffuse = dirDiff + sphDiff + spotDiff;
    outSpecular = dirSpec + sphSpec + spotSpec;
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