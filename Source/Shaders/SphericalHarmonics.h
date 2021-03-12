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


// "An Efficient Representation for Irradiance Environment Maps", Ravi Ramamoorthi, Pat Hanrahan


// Spherical harmonics coefficients.
// 4 coefficients (0, 1 bands) for each color channel.
// (L00, L1-1, L10, L11)
struct SH
{
    vec4 r;
    vec4 g;
    vec4 b;
};

SH newSH()
{
    SH sh;
    sh.r = sh.g = sh.b = vec4(0.0);

    return sh;
}

// Find SH coeffictients L00, L1-1, L10, L11 for each color channel.
// SH is a orthonormal basis, so inner product is used.
SH irradianceToSH(const vec3 color, const vec3 dir)
{
    const vec4 shBasis = vec4(
        // Y00
        0.282095,
        // Y1-1
        0.488603 * dir.y, 
        // Y10
        0.488603 * dir.z,
        // Y11
        0.488603 * dir.x 
    );

    SH sh;   
    
    // inner products of basis and color channels are the coefficients,
    // i.e. L_lm = <Y_lm, c>
    sh.r = shBasis * color.r;
    sh.g = shBasis * color.g;
    sh.b = shBasis * color.b;

    return sh;
}

// To find irradiance over hemisphere the integral Li(w)*dot(n,w)dw should be calculated.
// Spherical harmonics representation can be used to estimate it, but in frequency domain, 
// as it's less cpomplex to compute. After that, it's transformed back to space domain. 
vec3 SHToIrradiance(const SH sh, const vec3 normal)
{
    // A-hat includes inverse transorm and dot(n,w)
    float A_hat_0 = 3.141593;
    float A_hat_1 = 2.094395;

    // SH basis
    float Y_0_0 = 0.282095;
    float Y_1m1 = 0.488603 * normal.y; 
    float Y_1_0 = 0.488603 * normal.z;
    float Y_1_1 = 0.488603 * normal.x;

    // rather than separately calcualte E(n) for each color channel, 
    // combine them to RGB coefficients
    vec3 L_0_0 = vec3(sh.r[0], sh.g[0], sh.b[0]);
    vec3 L_1m1 = vec3(sh.r[1], sh.g[1], sh.b[1]); 
    vec3 L_1_0 = vec3(sh.r[2], sh.g[2], sh.b[2]);
    vec3 L_1_1 = vec3(sh.r[3], sh.g[3], sh.b[3]);

    // E(n)
    return A_hat_0 * L_0_0 * Y_0_0 +
           A_hat_1 * L_1m1 * Y_1m1 +
           A_hat_1 * L_1_0 * Y_1_0 +
           A_hat_1 * L_1_1 * Y_1_1;
}

vec3 getSHColor(const SH sh)
{
    return vec3(
        sh.r[0] / 0.282095,
        sh.g[0] / 0.282095,
        sh.b[0] / 0.282095
    );
}

void accumulateSH(inout SH x, const SH y, const float a)
{
    x.r += y.r * a;
    x.g += y.g * a;
    x.b += y.b * a;
}

SH mixSH(const SH x, const SH y, const float a)
{
    SH sh;
    sh.r = mix(x.r, y.r, a);
    sh.g = mix(x.g, y.g, a);
    sh.b = mix(x.b, y.b, a);

    return sh;
}

void multiplySH(inout SH x, float a)
{
    x.r *= a;
    x.g *= a;
    x.b *= a;
}