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


#define M_PI        3.14159265358979323846


// Sample disk uniformly
// u1, u2 -- uniform random numbers
vec2 sampleDisk(float radius, float u1, float u2)
{
    // polar mapping
    float r = radius * sqrt(u1);
    float phi = 2 * M_PI * u2;
    
    return vec2(
        r * cos(phi), 
        r * sin(phi)
    );

    // pdf = M_PI * radius * radius;
}

// Sample direction from cosine-weighted unit hemisphere oriented to Z axis
// u1, u2 -- uniform random numbers
vec3 sampleHemisphere(float u1, float u2)
{
    float r = sqrt(u1);
    float phi = 2 * M_PI * u2;

    return vec3( 
        r * cos(phi),
        r * sin(phi),
        sqrt(1 - u1)
    );

    // pdf = z / M_PI;
}

// "Building an Orthonormal Basis, Revisited"
void revisedONB(vec3 n, out vec3 b1, out vec3 b2)
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
void frisvadONB(vec3 n, out vec3 b1, out vec3 b2)
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

// Sample direction in a hemisphere oriented to a normal n
vec3 sampleOrientedHemisphere(vec3 n, float u1, float u2, out float pdf)
{
    vec3 a = sampleHemisphere(u1, u2);
    pdf = a.z / M_PI;

    mat3 basis;
    basis[2] = n;

    //revisedONB(n, basis[0], basis[1]);
    frisvadONB(n, basis[0], basis[1]);

    return normalize(basis * a);

    /*
    // Ray Tracing Gems, Chapter 16 "Sampling Transformations Zoo"
    float a = 1 - 2 * u1;
    float b = sqrt(1 - a * a);
    float phi = 2 * M_PI * u2;

    // ? may be negative
    pdf = a / M_PI;

    vec3 r = vec3(
        n.x + b * cos(phi),
        n.y + b * sin(phi),
        n.z + a
    );

    return normalize(r);*/
}


#ifdef DESC_SET_RANDOM
layout(
    set = DESC_SET_RANDOM,
    binding = BINDING_BLUE_NOISE)
    uniform sampler2DArray blueNoiseTextures;

#if BLUE_NOISE_TEXTURE_SIZE_POW * 2 > 31
    #error BLUE_NOISE_TEXTURE_SIZE_POW must be lower, around 6-8
#endif

uint packRandomSeed(uint textureIndex, uvec2 offset)
{
    return 
        textureIndex << (BLUE_NOISE_TEXTURE_SIZE_POW * 2) | 
        offset.y     <<  BLUE_NOISE_TEXTURE_SIZE_POW     | 
        offset.x;
}

void unpackRandomSeed(uint seed, out uint textureIndex, out uvec2 offset)
{
    textureIndex = seed >> (BLUE_NOISE_TEXTURE_SIZE_POW * 2);
    offset.y     = (seed >> BLUE_NOISE_TEXTURE_SIZE_POW) & (BLUE_NOISE_TEXTURE_SIZE - 1);
    offset.x     = seed                                  & (BLUE_NOISE_TEXTURE_SIZE - 1);
}

vec4 getBlueNoiseSample(uint x, uint y, uint index)
{
    return texelFetch(blueNoiseTextures, ivec3(x, y, index), 0);
}

vec4 getBlueNoiseSample(uint seed)
{
    uint texIndex;
    uvec2 offset;
    unpackRandomSeed(seed, texIndex, offset);

    return texelFetch(blueNoiseTextures, ivec3(offset.x, offset.y, texIndex), 0);
}

uint makeSeed(uint seed, uint salt)
{
    uint texIndex;
    uvec2 offset;
    unpackRandomSeed(seed, texIndex, offset);

    texIndex = (texIndex + salt) % BLUE_NOISE_TEXTURE_COUNT;

    return packRandomSeed(texIndex, offset);
}

uint getCurrentRandomSeed(ivec2 pix)
{
    uvec4 seed = texelFetch(framebufRandomSeed_Sampler, pix, 0);
    return seed.x;
}

uint getPreviousRandomSeed(ivec2 pix)
{
    uvec4 seed = texelFetch(framebufRandomSeed_Prev_Sampler, pix, 0);
    return seed.x;
}

vec2 sampleDisk(uint seed, float radius)
{
    // uniform distribution
    vec4 u = getBlueNoiseSample(seed);
    return sampleDisk(radius, u[0], u[1]);
}

vec3 sampleHemisphere(uint seed)
{
    vec4 u = getBlueNoiseSample(seed);
    return sampleHemisphere(u[0], u[1]);
}

uint getRandomSeed(ivec2 pix, uint frameIndex, float screenWidth, float screenHeight)
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