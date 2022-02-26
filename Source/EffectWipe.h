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

#include "EffectBase.h"
#include "Utils.h"

namespace RTGL1
{

struct EffectWipe final : public EffectBase
{
    struct PushConst
    {
        uint32_t stripWidthInPixels;
        uint32_t startFrameId;
        float beginTime;
        float endTime;
    };
    
    explicit EffectWipe(
        VkDevice device,
        const std::shared_ptr<const Framebuffers>  &framebuffers,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<const BlueNoise>     &blueNoise,
        const std::shared_ptr<const ShaderManager> &shaderManager)
    :
        EffectBase(device)
    {
        VkDescriptorSetLayout setLayouts[] =
        {
            framebuffers->GetDescSetLayout(),
            uniform->GetDescSetLayout(),
            blueNoise->GetDescSetLayout(),
        };

        InitBase(framebuffers, uniform, blueNoise, shaderManager, 
                 setLayouts, PushConst());
    }


    bool Setup(const RgDrawFrameWipeEffectParams *params, float currentTime, uint32_t currentFrameId, uint32_t screenWidth)
    {
        if (params == nullptr)
        {
            return false;
        }
        
        push.stripWidthInPixels = (uint32_t)(screenWidth * clamp(params->stripWidth, 0.0f, 1.0f));
        push.startFrameId = currentFrameId;
        push.beginTime = params->startTime;
        push.endTime   = params->startTime + params->duration;

        if (push.stripWidthInPixels == 0 || 
            push.beginTime >= push.endTime ||
            currentTime >= push.endTime)
        {
            return false;
        }

        return true;
    }

protected:
    const char *GetShaderName() override
    {
        return "EffectWipe";
    }

    bool GetPushConstData(uint8_t (&pData)[128], uint32_t *pDataSize) const override
    {
        memcpy(pData, &push, sizeof(push));
        *pDataSize = sizeof(push);
        return true;
    }

private:
    PushConst push;
};

}
