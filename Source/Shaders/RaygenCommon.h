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

#ifdef DESC_SET_TLAS 
#ifdef DESC_SET_GLOBAL_UNIFORM 
#ifdef DESC_SET_VERTEX_DATA 
#ifdef DESC_SET_TEXTURES
#ifdef DESC_SET_RANDOM 
#ifdef DESC_SET_LIGHT_SOURCES 
layout(binding = BINDING_ACCELERATION_STRUCTURE, set = DESC_SET_TLAS) uniform accelerationStructureEXT topLevelAS;


layout(location = PAYLOAD_INDEX_DEFAULT) rayPayloadEXT ShPayload payload;

#ifdef RAYGEN_SHADOW_PAYLOAD
layout(location = PAYLOAD_INDEX_SHADOW) rayPayloadEXT ShPayloadShadow payloadShadow;
#endif // RAYGEN_SHADOW_PAYLOAD


void resetPayload()
{
    payload.color = vec4(0.0);
    payload.baryCoords = vec2(0.0);
    payload.instIdAndIndex = 0;
    payload.geomAndPrimIndex = 0;
    payload.clsHitDistance = -1;
    payload.maxTransparDistance = -1;
}


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


#ifdef RAYGEN_SHADOW_PAYLOAD
// lightDirection is pointed to the light
bool castShadowRay(vec3 origin, vec3 lightDirection, uint cullMask)
{
    // prepare shadow payload
    payloadShadow.isShadowed = 1;  

    traceRayEXT(
        topLevelAS, 
        gl_RayFlagsSkipClosestHitShaderEXT, 
        cullMask, 
        0, 0, 	// sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_SHADOW, 		// shadow missIndex
        origin, 0.001, lightDirection, 10000.0, 
        PAYLOAD_INDEX_SHADOW);

    return payloadShadow.isShadowed == 1;
}

// viewDirection -- is direction to viewer
void processDirectionalLight(
    uint seed, vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    uint shadowCullMask,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint dirLightCount = globalUniform.lightSourceCountDirectional;

    if (dirLightCount == 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    float d = getBlueNoiseSample(seed).x;
    uint dirLightIndex = uint(d * dirLightCount);

    float oneOverPdf = dirLightCount;

    ShLightDirectional dirLight = lightSourcesDirecitional[dirLightIndex];

    vec2 disk = sampleDisk(seed, dirLight.tanAngularRadius);

    vec3 dir;
    {
        vec3 r = normalize(cross(dirLight.direction, vec3(0, 1, 0)));
        vec3 u = cross(r, dirLight.direction);
        dir = normalize(dirLight.direction + r * disk.x + u * disk.y);
    }

    float nl = dot(surfNormal, dir);
    float ngl = dot(surfNormalGeom,dir);

    if (nl <= 0 || ngl <= 0)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    bool isShadowed = castShadowRay(surfPosition, dir, shadowCullMask);

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

// viewDirection -- is direction to viewer
void processDirectIllumination(
    ivec2 pix, uint primaryInstCustomIndex, vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    out vec3 outDiffuse, out vec3 outSpecular)
{
    uint seed = getCurrentRandomSeed(pix);
    uint shadowCullMask = getShadowCullMask(primaryInstCustomIndex);

    processDirectionalLight(seed, surfPosition, surfNormal, surfNormalGeom, surfRoughness, viewDirection, shadowCullMask, outDiffuse, outSpecular);
}
#endif // RAYGEN_SHADOW_PAYLOAD
#endif // DESC_SET_LIGHT_SOURCES 
#endif // DESC_SET_TLAS 
#endif // DESC_SET_GLOBAL_UNIFORM 
#endif // DESC_SET_VERTEX_DATA 
#endif // DESC_SET_TEXTURES
#endif // DESC_SET_RANDOM 