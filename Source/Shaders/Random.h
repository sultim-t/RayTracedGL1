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

#ifndef RANDOM_H_
#define RANDOM_H_

#define RANDOM_SALT_LIGHT_TYPE_CHOOSE 1
#define RANDOM_SALT_DIRECTIONAL_LIGHT_DISK 2
#define RANDOM_SALT_LIGHT_CHOOSE 3
#define RANDOM_SALT_LIGHT_POINT 4
#define RANDOM_BOUNCE_DIFF_BASE_INDEX 8
#define RANDOM_SALT_DIFF_BOUNCE(bounceIndex) (RANDOM_BOUNCE_DIFF_BASE_INDEX + bounceIndex)
#define RANDOM_BOUNCE_SPEC_BASE_INDEX 16
#define RANDOM_SALT_SPEC_BOUNCE(bounceIndex) (RANDOM_BOUNCE_SPEC_BASE_INDEX + bounceIndex)

// Sample disk uniformly
// u1, u2 -- uniform random numbers
vec2 sampleDisk(float radius, float u1, float u2)
{
    // from [0,1] to [0,1)
    u1 *= 0.99;
    u2 *= 0.99;

    // polar mapping
    const float r = radius * sqrt(u1);
    const float phi = 2 * M_PI * u2;
    
    return vec2(
        r * cos(phi), 
        r * sin(phi)
    );

    // pdf = M_PI * radius * radius;
}

// Sample triangle uniformly
// u1, u2 -- uniform random numbers
// "Ray Tracing Gems", Chapter 16: Sampling Transformations Zoo,
// 16.5.2.1 Warping
vec3 sampleTriangle(const vec3 p0, const vec3 p1, const vec3 p2, float u1, float u2)
{
    // from [0,1] to [0,1)
    u1 *= 0.99;
    u2 *= 0.99;

    float beta = 1 - sqrt(u1);
    float gamma = (1 - beta) * u2;
    float alpha = 1 - beta - gamma;
    
    return alpha * p0 + beta * p1 + gamma * p2;
}

// Sample direction from cosine-weighted unit hemisphere oriented to Z axis
// u1, u2 -- uniform random numbers
vec3 sampleHemisphere(float u1, float u2, out float oneOverPdf)
{
    // from [0,1] to [0,1)
    u1 *= 0.99;
    u2 *= 0.99;

    const float r = sqrt(u1);
    const float phi = 2 * M_PI * u2;

    const float z = sqrt(1 - u1);

    // clamp z, so max oneOverPdf is finite (currenty, 10pi)
    oneOverPdf = M_PI / max(z, 0.1);

    return vec3( 
        r * cos(phi),
        r * sin(phi),
        z
    );
}

// Sample a surface point on a unit sphere with given radius
// u1, u2 -- uniform random numbers
// "Ray Tracing Gems", Chapter 16: Sampling Transformations Zoo,
// Octathedral concentric uniform map
vec3 sampleSphere(float u1, float u2)
{
    // from [0,1] to [0,1)
    u1 *= 0.99;
    u2 *= 0.99;

    u1 = 2 * u1 - 1;
    u2 = 2 * u2 - 1;

    const float d = 1 - (abs(u1) + abs(u2));
    const float r = 1 - abs(d);

    const float phi = r == 0 ? 0 : M_PI / 4 * ((abs(u2) - abs(u1)) / r + 1);
    const float f = r * sqrt(2 - r * r);

    return vec3(
        f * sign(u1) * cos(phi),
        f * sign(u2) * sin(phi),
        sign(d) * (1 - r * r));

    // pdf = 1 / (4 * M_PI)
}

// "Building an Orthonormal Basis, Revisited"
void revisedONB(const vec3 n, out vec3 b1, out vec3 b2)
{
    if(n.z < 0.0)
    {
        const float a = 1.0f / (1.0f - n.z);
        const float b = n.x * n.y * a;

        b1 = vec3(1.0f - n.x * n.x * a, -b, n.x);
        b2 = vec3(b, n.y * n.y * a - 1.0f, -n.y);
    }
    else
    {
        const float a = 1.0f / (1.0f + n.z);
        const float b = -n.x * n.y * a;

        b1 = vec3(1.0f - n.x * n.x * a, b, -n.x);
        b2 = vec3(b, 1.0f - n.y * n.y * a, -n.y);
    }
}

// "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization", Frisvad
void frisvadONB(const vec3 n, out vec3 b1, out vec3 b2)
{
    if(n.z < -0.9999999)
    {
        b1 = vec3( 0.0, -1.0, 0.0);
        b2 = vec3( -1.0, 0.0, 0.0);
        
        return;
    }

    const float a = 1.0 / (1.0 + n.z);
    const float b = -n.x * n.y * a;

    b1 = vec3(1.0 - n.x * n.x * a, b, n.x);
    b2 = vec3(b, 1.0 - n.y * n.y * a, -n.y);
}

mat3 getONB(const vec3 n)
{
    mat3 basis;
    basis[2] = n;

    //revisedONB(n, basis[0], basis[1]);
    frisvadONB(n, basis[0], basis[1]);

    return basis;
}

// Sample direction in a hemisphere oriented to a normal n
vec3 sampleOrientedHemisphere(const vec3 n, float u1, float u2, out float oneOverPdf)
{
    /*vec3 a = sampleHemisphere(u1, u2, oneOverPdf);

    mat3 basis = getONB(n);
    return normalize(basis * a);*/

    // Ray Tracing Gems, Chapter 16 "Sampling Transformations Zoo"
    float a = 1 - 2 * u1;
    float b = sqrt(1 - a * a);
    float phi = 2 * M_PI * u2;

    // avoid grazing angles (perpendicular to normal), 
    // so r won't be close to zero
    a *= 0.98;
    b *= 0.98;

    vec3 r = vec3(
        n.x + b * cos(phi),
        n.y + b * sin(phi),
        n.z + a
    );
    r = normalize(r);

    float z = dot(r, n);
    oneOverPdf = M_PI / max(z, 0.1);

    return r;
}


#ifdef DESC_SET_RANDOM
layout(
    set = DESC_SET_RANDOM,
    binding = BINDING_BLUE_NOISE)
    uniform texture2DArray blueNoiseTextures;

#if BLUE_NOISE_TEXTURE_SIZE_POW * 2 > 31
    #error BLUE_NOISE_TEXTURE_SIZE_POW must be lower, around 6-8
#endif

uint packRandomSeed(uint textureIndex, uvec2 offset)
{
    return 
        (textureIndex << (BLUE_NOISE_TEXTURE_SIZE_POW * 2)) | 
        (offset.y     << (BLUE_NOISE_TEXTURE_SIZE_POW    )) | 
        offset.x;
}

void unpackRandomSeed(uint seed, out uint textureIndex, out uvec2 offset)
{
    textureIndex = seed >> (BLUE_NOISE_TEXTURE_SIZE_POW * 2);
    offset.y     = (seed >> BLUE_NOISE_TEXTURE_SIZE_POW) & (BLUE_NOISE_TEXTURE_SIZE - 1);
    offset.x     = seed                                  & (BLUE_NOISE_TEXTURE_SIZE - 1);
}

// get blue noise sample
vec4 getRandomSample(uint seed, uint salt)
{
    uint texIndex;
    uvec2 offset;
    unpackRandomSeed(seed, texIndex, offset);

    texIndex = (texIndex + salt) % BLUE_NOISE_TEXTURE_COUNT;

    return texelFetch(blueNoiseTextures, ivec3(offset.x, offset.y, texIndex), 0);
}

uint getCurrentRandomSeed(const ivec2 pix)
{
    uvec4 seed = texelFetch(framebufRandomSeed_Sampler, pix, 0);
    return seed.x;
}

uint getRandomSeed(const ivec2 pix, uint frameIndex, float screenWidth, float screenHeight)
{
    uint idX = pix.x / BLUE_NOISE_TEXTURE_SIZE;
    uint idY = pix.y / BLUE_NOISE_TEXTURE_SIZE;

    uint countX = uint(ceil(screenWidth / BLUE_NOISE_TEXTURE_SIZE));
    uint countY = uint(ceil(screenHeight / BLUE_NOISE_TEXTURE_SIZE));

    uint texIndex = idY * countX + idX;
    texIndex = (texIndex + frameIndex) % BLUE_NOISE_TEXTURE_COUNT;
    
    uvec2 offset = uvec2(pix.x % BLUE_NOISE_TEXTURE_SIZE,
                         pix.y % BLUE_NOISE_TEXTURE_SIZE);

    return packRandomSeed(texIndex, offset);
}
#endif // DESC_SET_RANDOM

#endif // RANDOM_H_