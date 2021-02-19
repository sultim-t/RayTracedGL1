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
vec3 sampleLambertian(vec3 n, float u1, float u2, out float pdf)
{
    return sampleOrientedHemisphere(n, u1, u2, pdf);
}



// nl -- cos between surface normal and light direction
// n1 -- refractive index of src media
// n2 -- refractive index of dst media
float getFresnelSchlick(float nl, float n1, float n2)
{
    float F0 = square((n1 - n2) / (n1 + n2));
    return F0 + (1 - F0) * pow(1 - max(nl, 0), 5);
}

// Smith G1 for GGX, Karis' approximation ("Real Shading in Unreal Engine 4")
// s -- is either l or v
float G1GGX(vec3 s, vec3 n, float alpha)
{
    return 2 * dot(n, s) / (dot(n, s) * (2 - alpha) + alpha);
}

#define MIN_GGX_ROUGHNESS 0.001

// n -- macrosurface normal
// v -- direction to viewer
// l -- direction to light
// alpha -- roughness
float evalBRDFSmithGGX(vec3 n, vec3 v, vec3 l, float alpha)
{
    alpha = max(alpha, MIN_GGX_ROUGHNESS);

    float nl = dot(n, l);

    if (nl <= 0)
    {
        return 0;
    }

    vec3 h = normalize(v + l);

    nl = max(nl, 0);
    float nv = max(dot(n, v), 0);
    float nh = max(dot(n, h), 0);

    float alphaSq = alpha * alpha;

    float F = 1; // getFresnelSchlick(nl, n1, n2);
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
vec3 sampleGGXVNDF(vec3 v, float alpha, float u1, float u2)
{
    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha * v.x, alpha * v.y, v.z));
    
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
    vec3 T2 = cross(Vh, T1);

    // Section 4.2: parameterization of the projected area
    float r = sqrt(u1);    
    float phi = 2.0 * M_PI * u2;    
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    // Section 4.3: reprojection onto hemisphere
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    vec3 Ne = normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));    
    
    return Ne;
}

// Sample microfacet normal
// n        -- macrosurface normal, world space
// v        -- direction to viewer, world space
// alpha    -- roughness
// u1, u2   -- uniform random numbers
// pdf      -- PDF of sampled normal
vec3 sampleSmithGGX(vec3 n, vec3 v, float alpha, float u1, float u2/*, out float pdf*/)
{
    alpha = max(alpha, MIN_GGX_ROUGHNESS);

    mat3 basis = getONB(n);

    // get v in normal's space, basis is orthogonal
    vec3 ve = transpose(basis) * v;

    // microfacet normal
    vec3 me = sampleGGXVNDF(ve, alpha, u1, u2);

    // m to world space
    return basis * me; 

    /*vec3 m = basis * me;

    float nm = dot(n, m);

    if (nm <= 0)
    {
        pdf = 0;
        return vec3(0.0);
    }

    float G1 = G1GGX(ve, n, alpha);

    float alphaSq = alpha * alpha;

    // D for microfacet normal
    // D(me)
    float c = (me.x * me.x + me.y * me.y) / alphaSq + me.z * me.z;
    float D = 1.0 / (M_PI * alphaSq * c * c);
    
    // VNDF PDF
    pdf = G1 * max(0, dot(ve, me)) * D / ve.z;

    return m;*/
}
