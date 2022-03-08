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

#pragma once

#include "EffectSimple.h"

namespace RTGL1
{

struct EffectRadialBlur_PushConst
{};

struct EffectRadialBlur final : public EffectSimple<EffectRadialBlur_PushConst>
{
    RTGL1_EFFECT_SIMPLE_INHERIT_CONSTRUCTOR(EffectRadialBlur, "EffectRadialBlur")

        bool Setup(const CommonnlyUsedEffectArguments &args, const RgPostEffectRadialBlur *params)
    {
        if (params == nullptr)
        {
            return SetupNull();
        }

        return EffectSimple::Setup(args, params->isActive, params->transitionDurationIn, params->transitionDurationOut);
    }
};


// ------------------ //


struct EffectChromaticAberration_PushConst
{
    float intensity;
};

struct EffectChromaticAberration final : public EffectSimple<EffectChromaticAberration_PushConst>
{
    RTGL1_EFFECT_SIMPLE_INHERIT_CONSTRUCTOR(EffectChromaticAberration, "EffectChromaticAberration")

    bool Setup(const CommonnlyUsedEffectArguments &args, const RgPostEffectChromaticAberration *params)
    {
        if (params == nullptr || params->intensity <= 0.0f)
        {
            return SetupNull();
        }

        GetPush().intensity = params->intensity;
        return EffectSimple::Setup(args, params->isActive, params->transitionDurationIn, params->transitionDurationOut);
    }
};


// ------------------ //


struct EffectInverseBW_PushConst
{};

struct EffectInverseBW final : public EffectSimple<EffectInverseBW_PushConst>
{
    RTGL1_EFFECT_SIMPLE_INHERIT_CONSTRUCTOR(EffectInverseBW, "EffectInverseBW")

    bool Setup(const CommonnlyUsedEffectArguments &args, const RgPostEffectInverseBlackAndWhite *params)
    {
        if (params == nullptr)
        {
            return SetupNull();
        }
        
        return EffectSimple::Setup(args, params->isActive, params->transitionDurationIn, params->transitionDurationOut);
    }
};

}