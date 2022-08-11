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

#define RANDOM_SALT_DIFF_BOUNCE(bounceIndex) (8 + (bounceIndex))
#define RANDOM_SALT_SPEC_BOUNCE(bounceIndex) (12 + (bounceIndex))
#define RANDOM_SALT_POSTEFFECT 16
#define RANDOM_SALT_LIGHT_POINT 20
#define RANDOM_SALT_LIGHT_GRID_BASE 24
#define RANDOM_SALT_INITIAL_RESERVOIRS_BASE 48
#define RANDOM_SALT_LIGHT_CHOOSE_DIRECT_BASE 72
#define RANDOM_SALT_LIGHT_CHOOSE_INDIRECT_BASE 96

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

// Blue noise random in [0..1] with 1/255 precision
vec4 rndBlueNoise8(uint seed, uint salt)
{
    uint texIndex;
    uvec2 offset;
    unpackRandomSeed(seed, texIndex, offset);

    texIndex = (texIndex + salt) % BLUE_NOISE_TEXTURE_COUNT;

    return texelFetch(blueNoiseTextures, ivec3(offset.x, offset.y, texIndex), 0);
}

// https://nullprogram.com/blog/2018/07/31/
uint wellonsLowBias32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// Random in [0..1] with 1/65535 precision
float rnd16(uint seed, uint salt)
{
    uint rnd = wellonsLowBias32(seed + salt);
    return 
        float((rnd & 0x0000FFFF)      ) / float(UINT16_MAX);
}

vec2 rnd16_2(uint seed, uint salt)
{
    uint rnd = wellonsLowBias32(seed + salt);
    return vec2(
        float((rnd & 0x0000FFFF)      ) / float(UINT16_MAX),
        float((rnd & 0xFFFF0000) >> 16) / float(UINT16_MAX));
}

vec4 rnd8_4(uint seed, uint salt)
{
    uint rnd = wellonsLowBias32(seed + salt) % UINT16_MAX;
    return vec4(
        float((rnd & 0x000000FF)      ) / float(UINT8_MAX),
        float((rnd & 0x0000FF00) >> 8 ) / float(UINT8_MAX),
        float((rnd & 0x00FF0000) >> 16) / float(UINT8_MAX),
        float((rnd & 0xFF000000) >> 24) / float(UINT8_MAX));
}

// https://gist.github.com/mpottinger/54d99732d4831d8137d178b4a6007d1a
uvec3 murmurHash33(uvec3 src) {
    const uint M = 0x5bd1e995u;
    uvec3 h = uvec3(1190494759u, 2147483647u, 3559788179u);
    src *= M; src ^= src>>24u; src *= M;
    h *= M; h ^= src.x; h *= M; h ^= src.y; h *= M; h ^= src.z;
    h ^= h>>13u; h *= M; h ^= h>>15u;
    return h;
}

uint getRandomSeed(const ivec2 pix, uint frameIndex)
{
    uvec3 hash = murmurHash33(uvec3(pix.x, pix.y, frameIndex));

    uvec2 offset = uvec2(
        hash.x % BLUE_NOISE_TEXTURE_SIZE,
        hash.y % BLUE_NOISE_TEXTURE_SIZE
    );
    uint texIndex = hash.z % BLUE_NOISE_TEXTURE_COUNT;

    return packRandomSeed(texIndex, offset);
}
#endif // DESC_SET_RANDOM

#endif // RANDOM_H_