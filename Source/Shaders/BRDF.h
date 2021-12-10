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

#include "Random.h"

float square(float x)
{
    return x * x;
}



// subsurfaceAlbedo -- 0 if all light is absorbed,
//                     1 if no light is absorbed
float evalBRDFLambertian(float subsurfaceAlbedo)
{
    return subsurfaceAlbedo / M_PI;
}

// u1, u2   -- uniform random numbers
vec3 sampleLambertian(const vec3 n, float u1, float u2, out float oneOverPdf)
{
    return sampleOrientedHemisphere(n, u1, u2, oneOverPdf);
}



#define BRDF_MIN_SPECULAR_COLOR 0.0

vec3 getSpecularColor(const vec3 albedo, float metallic)
{
    vec3 minSpec = vec3(BRDF_MIN_SPECULAR_COLOR);
    return mix(minSpec, albedo, metallic);
}

// nl -- cos between surface normal and light direction
// specularColor -- reflectance color at zero angle
vec3 getFresnelSchlick(float nl, const vec3 specularColor)
{
    return specularColor + (vec3(1.0) - specularColor) * pow(1 - max(nl, 0), 5);
}

float getFresnelSchlick(float n1, float n2, const vec3 V, const vec3 N)
{
    float R0 = (n1 - n2) / (n1 + n2);
    R0 *= R0;

    return mix(R0, 1.0, pow(1.0 - abs(dot(N, V)), 5.0));
}

// Smith G1 for GGX, Karis' approximation ("Real Shading in Unreal Engine 4")
// s -- is either l or v
float G1GGX(const vec3 s, const vec3 n, float alpha)
{
    return 2 * dot(n, s) / (dot(n, s) * (2 - alpha) + alpha);
}

#define MIN_GGX_ROUGHNESS 0.02

// n -- macrosurface normal
// v -- direction to viewer
// l -- direction to light
// alpha -- roughness
vec3 evalBRDFSmithGGX(const vec3 n, const vec3 v, const vec3 l, float alpha, const vec3 specularColor)
{
    alpha = max(alpha, MIN_GGX_ROUGHNESS);

    float nl = dot(n, l);

    if (nl <= 0)
    {
        return vec3(0.0);
    }

    const vec3 h = normalize(v + l);

    nl = max(nl, 0);
    float nv = max(dot(n, v), 0);
    float nh = max(dot(n, h), 0);

    float alphaSq = alpha * alpha;

    const vec3 F = getFresnelSchlick(nl, specularColor);
    float D = nh * alphaSq / (M_PI * square(1 + nh * nh * (alphaSq - 1)));

    // approximation for SmithGGX, Hammon ("PBR Diffuse Lighting for GGX+Smith Microsurfaces")
    // inlcudes 1 / (4 * nl * nv)
    float G2Modif = 0.5 / mix(2 * nl * nv, nl + nv, alpha);

    return F * G2Modif * D;
}



// "Sampling the GGX Distribution of Visible Normals", Heitz
// v        -- direction to viewer, normal's direction is (0,0,1)
// alpha    -- roughness
// u1, u2   -- uniform random numbers
// output   -- normal sampled with PDF D_v(Ne) = G1(v) * max(0, dot(v, Ne)) * D(Ne) / v.z
vec3 sampleGGXVNDF(const vec3 v, float alpha, float u1, float u2)
{
    // fix: avoid grazing angles
    u1 *= 0.98;
    u2 *= 0.98;

    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha * v.x, alpha * v.y, v.z));
    
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    const vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
    const vec3 T2 = cross(Vh, T1);

    // Section 4.2: parameterization of the projected area
    float r = sqrt(u1);    
    float phi = 2.0 * M_PI * u2;    
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    // Section 4.3: reprojection onto hemisphere
    const vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    const vec3 Ne = normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));    
    
    return Ne;
}

// Sample microfacet normal
// n        -- macrosurface normal, world space
// v        -- direction to viewer, world space
// alpha    -- roughness
// u1, u2   -- uniform random numbers
// Check Heitz's paper for the special representation of rendering equation term 
vec3 sampleSmithGGX(const vec3 n, const vec3 v, float alpha, float u1, float u2)
{
    if (alpha < MIN_GGX_ROUGHNESS)
    {
        return n;
    }

    alpha = max(alpha, MIN_GGX_ROUGHNESS);

    const mat3 basis = getONB(n);

    // get v in normal's space, basis is orthogonal
    const vec3 ve = transpose(basis) * v;

    // microfacet normal
    const vec3 me = sampleGGXVNDF(ve, alpha, u1, u2);

    // m to world space
    return basis * me; 
}
