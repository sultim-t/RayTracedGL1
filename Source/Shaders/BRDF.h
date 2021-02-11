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
    vec3 d = sampleHemisphere(n, u1, u2);
    pdf = d.z / M_PI;

    return d;
}



// nl -- cos between surface normal and light direction
// n1 -- refractive index of src media
// n2 -- refractive index of dst media
float getFresnelSchlick(float nl, float n1, float n2)
{
    float F0 = square((n1 - n2) / (n1 + n2));
    return F0 + (1 - F0) * pow(1 - max(nl, 0), 5);
}

// n -- macrosurface normal
// v -- direction to viewer
// l -- direction to light
// alpha -- roughness
float evalBRDFSmithGGX(vec3 n, vec3 v, vec3 l, float alpha)
{
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
// v        -- direction to viewer
// alpha    -- roughness
// u1, u2   -- uniform random numbers
// output   -- normal sampled with PDF D_v(Ne) = G1(v) * max(0, dot(v, Ne)) * D(Ne) / v.z
vec3 sampleGGXVNDF(vec3 v, float alpha, float u1, float u2)
{
    float alpha_x = alpha;
    float alpha_y = alpha;

    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha_x * v.x, alpha_y * v.y, v.z));
    
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
    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));    
    
    return Ne;
}

// Sample microfacet normal
// n        -- macrosurface normal
// v        -- direction to viewer
// alpha    -- roughness
// u1, u2   -- uniform random numbers
// outPdf   -- PDF of sampled normal
vec3 sampleSmithGGX(vec3 n, vec3 v, float alpha, float u1, float u2, out float outPdf)
{
    // microfacet normal
    vec3 m = sampleGGXVNDF(v, alpha, u1, u2);

    float nm = dot(n, m);

    if (nm <= 0)
    {
        pdf = 0;
        return;
    }

    // Smith G1 for GGX, Karis' approximation ("Real Shading in Unreal Engine 4")
    float G1 = 2 * dot(n, v) / (dot(n, v) * (2 - alpha) + alpha);

    float alphaSq = alpha * alpha;

    // D for microfacet normal
    float D = nm * alphaSq / (M_PI * square(1 + nm * nm * (alphaSq - 1)));

    // VNDF PDF
    pdf = G1 * max(0, dot(v, m)) * D / v.z;
}