#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;


layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 2) uniform UBO 
{
	mat4 viewInverse;
	mat4 projInverse;
	vec4 lightPos;
} ubo;

#define MAX_STATIC_VERTICES (1 << 21)
#define ALIGN_SIZE_4(x, n)  ((x * n + 3) & (~3))

layout(binding = 0, set = 3) buffer Vertices 
{ 
    float positions[ALIGN_SIZE_4(MAX_STATIC_VERTICES, 3)];
    float normals[ALIGN_SIZE_4(MAX_STATIC_VERTICES, 3)];
} vertices;


vec3 getPosition(int i)
{
	return vec3(
		vertices.positions[i * 3 + 0],
		vertices.positions[i * 3 + 1],
		vertices.positions[i * 3 + 2]);
}


void main()
{
	//ivec3 index = ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1], indices.i[3 * gl_PrimitiveID + 2]);
	ivec3 index = ivec3(gl_PrimitiveID * 3 + 0, gl_PrimitiveID * 3 + 1, gl_PrimitiveID * 3 + 2);

	vec3 v0 = getPosition(index.x);
	vec3 v1 = getPosition(index.y);
	vec3 v2 = getPosition(index.z);

	// Interpolate normal
	//const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	//vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	vec3 normal = normalize(cross(v1 - v0, v2 - v0));

	// Basic lighting
	vec3 lightVector = normalize(ubo.lightPos.xyz);
	float dot_product = max(dot(lightVector, normal), 0.2);
	hitValue = vec3(dot_product);
 
	// Shadow casting
	float tmin = 0.001;
	float tmax = 10000.0;

	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

	shadowed = true;  
	// Trace shadow ray and offset indices to match shadow hit/miss shader group indices
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, origin, tmin, lightVector, tmax, 2);
	
	if (shadowed) 
	{
		hitValue *= 0.3;
	}
}
