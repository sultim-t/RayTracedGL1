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
layout(set = DESC_SET_TLAS, binding = BINDING_ACCELERATION_STRUCTURE_SKYBOX) uniform accelerationStructureEXT skyboxTopLevelAS;

#ifdef DESC_SET_CUBEMAPS
layout(set = DESC_SET_CUBEMAPS, binding = BINDING_CUBEMAPS) uniform samplerCube globalCubemaps[];
#endif

layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload payload;

#ifdef RAYGEN_SHADOW_PAYLOAD
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayloadShadow payloadShadow;
#endif // RAYGEN_SHADOW_PAYLOAD


uint getPrimaryVisibilityCullMask()
{
    return INSTANCE_MASK_ALL & (~INSTANCE_MASK_FIRST_PERSON_VIEWER);
}

uint getShadowCullMask(uint primaryInstCustomIndex)
{
    if ((primaryInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON) != 0)
    {
        // no first-person viewer shadows -- on first-person
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON;
    }
    else if ((primaryInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER) != 0)
    {
        // no first-person shadows -- on first-person viewer
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    else
    {
        // no first-person shadows -- on world
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    
    // blended geometry doesn't have shadows
}

uint getIndirectIlluminationCullMask(uint primaryInstCustomIndex)
{
    if ((primaryInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON) != 0)
    {
        // no first-person viewer indirect illumination -- on first-person
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON;
    }
    else if ((primaryInstCustomIndex & INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER) != 0)
    {
        // no first-person indirect illumination -- on first-person viewer
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    else
    {
        // no first-person indirect illumination -- on first-person viewer
        return INSTANCE_MASK_WORLD | INSTANCE_MASK_FIRST_PERSON_VIEWER;
    }
    
    // blended geometry doesn't have indirect illumination
}


void resetPayload()
{
    payload.color = vec4(0.0);
    payload.baryCoords = vec2(0.0);
    payload.instIdAndIndex = 0;
    payload.geomAndPrimIndex = 0;
    payload.clsHitDistance = -1;
    payload.maxTransparDistance = -1;
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

ShPayload traceIndirectRay(uint primaryInstCustomIndex, vec3 surfPosition, vec3 bounceDirection)
{
    resetPayload();

    uint cullMask = getIndirectIlluminationCullMask(primaryInstCustomIndex);

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
ShPayload traceSkyRay(vec3 origin, vec3 direction)
{
    resetPayload();

    uint cullMask = INSTANCE_MASK_SKYBOX;

    traceRayEXT(
        skyboxTopLevelAS,
        gl_RayFlagsNoneEXT, 
        cullMask, 
        0, 0,     // sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_DEFAULT, 
        origin, 0.001, direction, MAX_RAY_LENGTH, 
        PAYLOAD_INDEX_DEFAULT); 

    return payload;
}

// Get sky color for primary visibility, i.e. without skyColorMultiplier
vec3 getSkyPrimary(vec3 direction)
{
    uint skyType = globalUniform.skyType;

    if (skyType == SKY_TYPE_TLAS)
    {
        ShPayload p = traceSkyRay(globalUniform.skyViewerPosition.xyz, direction);

        if (p.clsHitDistance > 0)
        {
            return getHitInfoAlbedoOnly(p);
        }
    }
    else if (skyType == SKY_TYPE_CUBEMAP)
    {
        return texture(globalCubemaps[nonuniformEXT(globalUniform.skyCubemapIndex)], direction).rgb;
    }
    
    return globalUniform.skyColorDefault.xyz;
}

vec3 getSky(vec3 direction)
{
    uint skyType = globalUniform.skyType;

    if (skyType == SKY_TYPE_TLAS)
    {
        ShPayload p = traceSkyRay(globalUniform.skyViewerPosition.xyz, direction);

        if (p.clsHitDistance > 0)
        {
            return getHitInfoAlbedoOnly(p) * globalUniform.skyColorMultiplier;
        }
    }
    else if (skyType == SKY_TYPE_CUBEMAP)
    {
        return texture(globalCubemaps[nonuniformEXT(globalUniform.skyCubemapIndex)], direction).rgb
            * globalUniform.skyColorMultiplier;
    }
    
    return globalUniform.skyColorDefault.xyz * globalUniform.skyColorMultiplier;
}
#endif

#ifdef RAYGEN_SHADOW_PAYLOAD
// l is pointed to the light
bool traceShadowRay(uint primaryInstCustomIndex, vec3 o, vec3 l, float maxDistance)
{
    // prepare shadow payload
    payloadShadow.isShadowed = 1;  

    uint cullMask = getShadowCullMask(primaryInstCustomIndex);

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

bool traceShadowRay(uint primaryInstCustomIndex, vec3 o, vec3 l)
{
    return traceShadowRay(primaryInstCustomIndex, o, l, MAX_RAY_LENGTH);
}

// viewDirection -- is direction to viewer
void processDirectionalLight(
    uint seed, uint primaryInstCustomIndex,
    vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    out vec3 outDiffuse, out vec3 outSpecular)
{
    const uint dirLightCount = globalUniform.lightSourceCountDirectional;

    if (dirLightCount == 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const float randomIndex = getRandomSample(seed, RANDOM_SALT_DIRECTIONAL_LIGHT_INDEX).x;
    const uint dirLightIndex = clamp(uint(randomIndex * dirLightCount), 0, dirLightCount - 1);

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

    const bool isShadowed = traceShadowRay(primaryInstCustomIndex, surfPosition, dir);

    if (isShadowed)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    outDiffuse = evalBRDFLambertian(1.0) * dirLight.color * nl * M_PI;
    outSpecular = evalBRDFSmithGGX(surfNormal, viewDirection, dirLight.direction, surfRoughness) * dirLight.color * nl;

    outDiffuse *= oneOverPdf;
    outSpecular *= oneOverPdf;
}

void processSphericalLight(
    uint seed, uint primaryInstCustomIndex,
    vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    out vec3 outDiffuse, out vec3 outSpecular)
{
    const uint sphLightCount = globalUniform.lightSourceCountSpherical;

    if (sphLightCount == 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const float randomIndex = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_INDEX).x;
    const uint sphLightIndex = clamp(uint(randomIndex * sphLightCount), 0, sphLightCount - 1);
    
    const float oneOverPdf = sphLightCount;

    const ShLightSpherical sphLight = lightSourcesSpherical[sphLightIndex];

    const vec2 u = getRandomSample(seed, RANDOM_SALT_SPHERICAL_LIGHT_DISK).xy;

    const vec3 posOnSphere = sphLight.position + sampleSphere(u[0], u[1]) * sphLight.radius;
    vec3 dir = posOnSphere - surfPosition;
    float distance = max(length(dir), 0.0001);
    dir = dir / distance;

    const bool isShadowed = traceShadowRay(primaryInstCustomIndex, surfPosition, dir, distance);

    if (isShadowed)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    const float r = max(sphLight.radius, 0.1);
    
    const vec3 l = sphLight.position - surfPosition;
    const float d = max(length(l), r);
    
    const vec3 c = pow(max(0, 1 - pow(r / d, 2)), 2) * sphLight.color;

    const vec3 irradiance = M_PI * c * max(dot(surfNormal, l / d), 0.0);
    const vec3 radiance = evalBRDFLambertian(1.0) * irradiance;

    outDiffuse = radiance;
    outSpecular = evalBRDFSmithGGX(surfNormal, viewDirection, dir, surfRoughness) * sphLight.color * dot(surfNormal, dir);

    outDiffuse *= oneOverPdf;
    outSpecular *= oneOverPdf;
}

// viewDirection -- is direction to viewer
void processDirectIllumination(
    ivec2 pix, uint primaryInstCustomIndex, vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint seed = getCurrentRandomSeed(pix);

    vec3 dirDiff, dirSpec;
    processDirectionalLight(seed, primaryInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, viewDirection, dirDiff, dirSpec);
    
    vec3 sphDiff, sphSpec;
    processSphericalLight(seed, primaryInstCustomIndex, surfPosition, surfNormal, surfNormalGeom, surfRoughness, viewDirection, sphDiff, sphSpec);
    
    outDiffuse = dirDiff + sphDiff;
    outSpecular = dirSpec + sphSpec;
}
#endif // RAYGEN_SHADOW_PAYLOAD