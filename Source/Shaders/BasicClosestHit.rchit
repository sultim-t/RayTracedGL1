#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#include "ShaderCommonGLSLFunc.h"

layout(binding = BINDING_ACCELERATION_STRUCTURE, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = BINDING_TEXTURES, set = 4) uniform sampler2D textures[];


layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 inBaryCoords;


void main()
{
	vec3 baryCoords = vec3(1.0f - inBaryCoords.x - inBaryCoords.y, inBaryCoords.x, inBaryCoords.y);

	ShTriangle tr = getTriangle(gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT, gl_PrimitiveID);
	mat4 model = getModelMatrix(gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT);

    vec2 texCoord = tr.texCoords[0] * baryCoords.x + tr.texCoords[1] * baryCoords.y + tr.texCoords[2] * baryCoords.z;
  	vec3 color = vec3(1, 1, 1);

	//if (tr.materials[0][0] > 0)
	{
		color = texture(textures[nonuniformEXT(tr.materials[0][0])], texCoord).xyz;
	}  

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
	
	vec3 lightVec = normalize(vec3(1, 1, 1));
	hitValue = vec3(max(dot(lightVec, normal), 0.2)) * color;

	//hitValue = vec3((gl_GeometryIndexEXT % 8) / 8.0, 0, 0);
	//hitValue = vec3(0, (gl_PrimitiveID % 8) / 8.0, 0);

	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

	shadowed = true;  
	
	traceRayEXT(
		topLevelAS, 
		gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 
		0xFF, 
		0, 0, 	// sbtRecordOffset, sbtRecordStride
		1, 		// shadow missIndex
		origin, 0.001, lightVec, 10000.0, 
		2);		// shadow payload
	
	if (shadowed) 
	{
		hitValue *= 0.3;
	}
}
