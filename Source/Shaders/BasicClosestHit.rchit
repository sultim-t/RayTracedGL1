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

#version 460
#extension GL_EXT_ray_tracing : require

#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#define DESC_SET_TEXTURES 4
#include "ShaderCommonGLSLFunc.h"

layout(binding = BINDING_ACCELERATION_STRUCTURE, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) rayPayloadInEXT ShPayload payload;
layout(location = 1) rayPayloadEXT ShPayloadShadow payloadShadow;
hitAttributeEXT vec2 inBaryCoords;

void main()
{
	ShTriangle tr = getTriangle(gl_InstanceID, gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT, gl_PrimitiveID);
	mat4 model = getModelMatrix(gl_InstanceID, gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT);

	vec3 baryCoords = vec3(1.0f - inBaryCoords.x - inBaryCoords.y, inBaryCoords.x, inBaryCoords.y);
    vec2 texCoord = tr.texCoords[0] * baryCoords.x + tr.texCoords[1] * baryCoords.y + tr.texCoords[2] * baryCoords.z;
  	vec3 color = getTextureSample(tr.materials[0][0], texCoord).xyz;

	vec3 normal;

	if ((gl_InstanceCustomIndexEXT & INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC) == INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC)
	{

		tr.normals[0] = vec3(model * vec4(tr.normals[0], 0.0));
		tr.normals[1] = vec3(model * vec4(tr.normals[1], 0.0));
		tr.normals[2] = vec3(model * vec4(tr.normals[2], 0.0));

		normal = normalize(tr.normals[0] * baryCoords.x + tr.normals[1] * baryCoords.y + tr.normals[2] * baryCoords.z);
	}
	else
	{
		tr.positions[0] = vec3( vec4(tr.positions[0], 1.0));
		tr.positions[1] = vec3( vec4(tr.positions[1], 1.0));
		tr.positions[2] = vec3( vec4(tr.positions[2], 1.0));

		normal = normalize(cross(tr.positions[1] - tr.positions[0], tr.positions[2] - tr.positions[0]));
	}
	
	vec3 lightVec = normalize(vec3(1.0, 1.0, 1.0));
	flaot light = max(dot(lightVec, normal), 0.2);

	payload.color = vec4(light * color, 1.0);
	payloadShadow.isShadowed = true;  

	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	traceRayEXT(
		topLevelAS, 
		gl_RayFlagsSkipClosestHitShaderEXT, 
		0xFF, 
		0, 0, 	// sbtRecordOffset, sbtRecordStride
		1, 		// shadow missIndex
		origin, 0.001, lightVec, 10000.0, 
		1);		// shadow payload
	
	if (payloadShadow.isShadowed) 
	{
		payload.color *= 0.3;
	}
}
