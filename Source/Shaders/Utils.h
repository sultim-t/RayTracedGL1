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
#define UINT32_MAX  0xFFFFFFFF

vec4 unpackLittleEndianUintColor(uint c)
{
    return vec4(
         (c & 0x000000FF)        / 255.0,
        ((c & 0x0000FF00) >> 8)  / 255.0,
        ((c & 0x00FF0000) >> 16) / 255.0,
        ((c & 0xFF000000) >> 24) / 255.0
    );
}

float getLuminance(vec3 c)
{
    return 0.2125 * c.r + 0.7154 * c.g + 0.0721 * c.b;
}

#define ENCODE_NORMAL_N_PHI 1 << 16
#define ENCODE_NORMAL_N_THETA 1 << 16

uint encodeNormal(vec3 n)
{
    const uint N_phi = ENCODE_NORMAL_N_PHI;
    const uint N_theta = ENCODE_NORMAL_N_THETA;

    float phi = acos(n.z);
    // atan -> [-pi, pi], need [0, 2pi]
	float theta = atan(n.y, n.x);
    theta = theta < 0 ? theta + 2 * M_PI : theta;

    uint j = uint(round(phi * (N_phi - 1) / M_PI));
    uint k = uint(round(theta * N_theta / (2 * M_PI))) % N_theta;

    return (j << 16) | k;
}

vec3 decodeNormal(uint _packed)
{
    const uint N_phi = ENCODE_NORMAL_N_PHI;
    const uint N_theta = ENCODE_NORMAL_N_THETA;

    uint j = _packed >> 16;
    uint k = _packed & 0xFFFF;

    float phi = j * M_PI / (N_phi - 1);
    float theta = k * 2 * M_PI / N_theta;

    return vec3(
        sin(phi) * cos(theta),
        sin(phi) * sin(theta),
        cos(phi)
    );
}

// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt

#define ENCODE_E5B9G9R9_EXPONENT_BITS 5
#define ENCODE_E5B9G9R9_MANTISSA_BITS 9
#define ENCODE_E5B9G9R9_MAX_VALID_BIASED_EXP 31
#define ENCODE_E5B9G9R9_EXP_BIAS 15

#define ENCODE_E5B9G9R9_MANTISSA_VALUES (1 << 9)
#define ENCODE_E5B9G9R9_MANTISSA_MASK (ENCODE_E5B9G9R9_MANTISSA_VALUES - 1)
// Equals to (((float)(MANTISSA_VALUES - 1))/MANTISSA_VALUES * (1<<(MAX_VALID_BIASED_EXP-EXP_BIAS)))
#define ENCODE_E5B9G9R9_SHAREDEXP_MAX 65408

uint encodeE5B9G9R9(vec3 unpacked)
{
    const int N = ENCODE_E5B9G9R9_MANTISSA_BITS;
    const int Np2 = 1 << N;
    const int B = ENCODE_E5B9G9R9_EXP_BIAS;

    unpacked = clamp(unpacked, vec3(0.0), vec3(ENCODE_E5B9G9R9_SHAREDEXP_MAX));
    float max_c = max(unpacked.r, max(unpacked.g, unpacked.b));

    // for log2
    if (max_c == 0.0)
    {
        return 0;
    }

    int exp_shared_p = max(-B-1, int(floor(log2(max_c)))) + 1 + B;
    int max_s = int(round(max_c * exp2(-(exp_shared_p - B - N))));

    int exp_shared = max_s != Np2 ? 
        exp_shared_p : 
        exp_shared_p + 1;

    float s = exp2(-(exp_shared - B - N));
    uvec3 rgb_s = uvec3(round(unpacked * s));

    return 
        (exp_shared << (3 * ENCODE_E5B9G9R9_MANTISSA_BITS)) |
        (rgb_s.b    << (2 * ENCODE_E5B9G9R9_MANTISSA_BITS)) |
        (rgb_s.g    << (1 * ENCODE_E5B9G9R9_MANTISSA_BITS)) |
        (rgb_s.r);
}

vec3 decodeE5B9G9R9(const uint _packed)
{
    const int N = ENCODE_E5B9G9R9_MANTISSA_BITS;
    const int B = ENCODE_E5B9G9R9_EXP_BIAS;

    int exp_shared = int(_packed >> (3 * ENCODE_E5B9G9R9_MANTISSA_BITS));
    float s = exp2(exp_shared - B - N);

    return s * vec3(
        (_packed                                       ) & ENCODE_E5B9G9R9_MANTISSA_MASK, 
        (_packed >> (1 * ENCODE_E5B9G9R9_MANTISSA_BITS)) & ENCODE_E5B9G9R9_MANTISSA_MASK,
        (_packed >> (2 * ENCODE_E5B9G9R9_MANTISSA_BITS)) & ENCODE_E5B9G9R9_MANTISSA_MASK
    );
}