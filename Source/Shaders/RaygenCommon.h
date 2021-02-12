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

#define DESC_SET_FRAMEBUFFERS 1
#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#define DESC_SET_TEXTURES 4
#define DESC_SET_RANDOM 5
#include "ShaderCommonGLSLFunc.h"

layout(binding = BINDING_ACCELERATION_STRUCTURE, set = 0) uniform accelerationStructureEXT topLevelAS;


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

#ifdef RAYGEN_SHADOW_PAYLOAD
// lightDirection is pointed to the light
bool castShadowRay(vec3 origin, vec3 lightDirection)
{
    // prepare shadow payload
    payloadShadow.isShadowed = 1;  
    
    traceRayEXT(
        topLevelAS, 
        gl_RayFlagsSkipClosestHitShaderEXT, 
        INSTANCE_MASK_HAS_SHADOWS, 
        0, 0, 	// sbtRecordOffset, sbtRecordStride
        SBT_INDEX_MISS_SHADOW, 		// shadow missIndex
        origin, 0.001, lightDirection, 10000.0, 
        PAYLOAD_INDEX_SHADOW);

    return payloadShadow.isShadowed == 1;
}

struct ShDirectionalLight
{
    vec3 direction;
    float angularDiameterDegrees;
    vec3 color;
};

// viewDirection -- is direction to viewer
void processDirectionalLight(
    uint seed, vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    ShDirectionalLight dirLight,
    out vec3 outDiffuse, out vec3 outSpecular)
{
    vec2 disk = sampleDisk(seed, tan(radians(dirLight.angularDiameterDegrees * 0.5)));

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

    bool isShadowed = castShadowRay(surfPosition, dir);

    if (isShadowed)
    {
        outDiffuse = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    outDiffuse = evalBRDFLambertian(1.0) * dirLight.color * nl;
    outSpecular = evalBRDFSmithGGX(surfNormal, viewDirection, dirLight.direction, surfRoughness) * dirLight.color * nl;
}

// viewDirection -- is direction to viewer
void processDirectIllumination(
    ivec2 pix, vec3 surfPosition, 
    vec3 surfNormal, vec3 surfNormalGeom,
    float surfRoughness, vec3 viewDirection, 
    out vec3 outDiffuse, out vec3 outSpecular)
{
    ShDirectionalLight lt;
    lt.direction = normalize(vec3(-1, 1, 1));
    lt.angularDiameterDegrees = 0.5;
    lt.color = vec3(10, 10, 10);

    uint seed = getCurrentRandomSeed(pix);
    processDirectionalLight(seed, surfPosition, surfNormal, surfNormalGeom, surfRoughness, viewDirection, lt, outDiffuse, outSpecular);
}
#endif // RAYGEN_SHADOW_PAYLOAD