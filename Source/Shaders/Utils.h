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

const uint N_phi = 1 << 16;
const uint N_theta = 1 << 16;

uint encodeNormal(vec3 n)
{
    float phi = acos(n.z);
    // atan -> [-pi, pi], need [0, 2pi]
	float theta = atan(n.y, n.x);
    theta = theta < 0 ? theta + 2 * M_PI : theta;

    uint j = uint(round(phi * (N_phi - 1) / M_PI));
    uint k = uint(round(theta * N_theta / (2 * M_PI))) % N_theta;

    return (j << 16) | k;
}

vec3 decodeNormal(uint packed)
{
    uint j = packed >> 16;
    uint k = packed & 0xFFFF;

    float phi = j * M_PI / (N_phi - 1);
    float theta = k * 2 * M_PI / N_theta;

    return vec3(
        sin(phi) * cos(theta),
        sin(phi) * sin(theta),
        cos(phi)
    );
}