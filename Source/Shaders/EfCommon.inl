// Copyright (c) 2022 Sultim Tsyrendashiev
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


#if !defined(EFFECT_SOURCE_IS_PING) && !defined(EFFECT_SOURCE_IS_PONG) 
    #error Define EFFECT_SOURCE_IS_PING or EFFECT_SOURCE_IS_PONG to boolean value
#endif


ivec2 effect_getFramebufSize()
{
    return imageSize(framebufUpscaledPing); // framebufUpscaledPong has the same size
}


vec2 effect_getInverseFramebufSize()
{
    ivec2 sz = effect_getFramebufSize();
    return vec2(1.0 / float(sz.x), 1.0 / float(sz.y));
}


// get UV coords in [0..1] range
vec2 effect_getFramebufUV(ivec2 pix)
{
    return (vec2(pix) + 0.5) * effect_getInverseFramebufSize();
}


// to [-1..1]
vec2 effect_getCenteredFromPix(ivec2 pix)
{
    return effect_getFramebufUV(pix) * 2.0 - 1.0;
}


// from [-1..1]
ivec2 effect_getPixFromCentered(vec2 centered)
{
    return ivec2((centered * 0.5 + 0.5) * effect_getFramebufSize());
}


vec3 effect_loadFromSource(ivec2 pix)
{
    if (EFFECT_SOURCE_IS_PING)
    {
        return imageLoad(framebufUpscaledPing, pix).rgb;
    }
    else
    {
        return imageLoad(framebufUpscaledPong, pix).rgb;
    }
}


void effect_storeToTarget(const vec3 value, ivec2 pix)
{
    if (EFFECT_SOURCE_IS_PING)
    {
        imageStore(framebufUpscaledPong, pix, vec4(value, 0.0));
    }
    else
    {
        imageStore(framebufUpscaledPing, pix, vec4(value, 0.0));
    }
}


void effect_storeUnmodifiedToTarget(ivec2 pix)
{
    effect_storeToTarget(effect_loadFromSource(pix), pix);
}


vec3 effect_loadFromSource_Centered(vec2 centered)
{
    return effect_loadFromSource(effect_getPixFromCentered(centered));
}


#ifdef DESC_SET_RANDOM
vec4 effect_getRandomSample(ivec2 pix, uint frameIndex)
{
    ivec2 sz = effect_getFramebufSize();
    return getRandomSample(getRandomSeed(pix, frameIndex, sz.x, sz.y), 0);
}
#endif


// Need these functions as R10G11B10 doesn't allow negative values,
// and I/Q components can be <0
#define I_LIMIT 0.6
#define Q_LIMIT 0.55
vec3 encodeYiqForStorage(vec3 yiq)
{
    float i = clamp(yiq.y, -I_LIMIT, I_LIMIT);
    float q = clamp(yiq.z, -Q_LIMIT, Q_LIMIT);

    i += I_LIMIT;
    q += Q_LIMIT;

    i /= I_LIMIT * 2;
    q /= Q_LIMIT * 2;

    return vec3(yiq.x, i, q);
}
vec3 decodeYiqFromStorage(vec3 yiqFromStorage)
{
    float i = clamp(yiqFromStorage.y, 0, 1);
    float q = clamp(yiqFromStorage.z, 0, 1);

    i *= I_LIMIT * 2;
    q *= Q_LIMIT * 2;

    i -= I_LIMIT;
    q -= Q_LIMIT;

    return vec3(yiqFromStorage.x, i, q);
}
#undef I_LIMIT
#undef Q_LIMIT