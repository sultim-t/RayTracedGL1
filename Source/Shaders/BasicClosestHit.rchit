#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#include "ShaderCommonGLSLFunc.h"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;


layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 baryCoord;


void main()
{
	ShTriangle tr = getTriangle(gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT, gl_PrimitiveID);
	
	//vec3 barycentricCoords = vec3(1.0f - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);
	// vec3 normal = normalize(tr.normals[0] * barycentricCoords.x + tr.normals[1] * barycentricCoords.y + tr.normals[2] * barycentricCoords.z);

	vec3 normal = cross(tr.positions[1] - tr.positions[0], tr.positions[2] - tr.positions[0]);
	normal = normalize(normal);

	vec3 lightVec = normalize(vec3(1, 1, 1));
	hitValue = vec3(max(dot(-lightVec, normal), 0.2));

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
