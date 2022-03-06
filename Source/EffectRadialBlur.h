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

struct EffectRadialBlur final : public EffectBase
{
    struct PushConst
    {
        float beginTime;
        float endTime;
        float transitionDuration;
    };

    explicit EffectRadialBlur(
        VkDevice device,
        const std::shared_ptr<const Framebuffers> &framebuffers,
        const std::shared_ptr<const GlobalUniform> &uniform,
        const std::shared_ptr<const ShaderManager> &shaderManager)
    :
        EffectBase(device),
        push{}
    {
        VkDescriptorSetLayout setLayouts[] =
        {
            framebuffers->GetDescSetLayout(),
            uniform->GetDescSetLayout(),
        };

        InitBase(shaderManager, setLayouts, PushConst());
    }

    bool Setup(VkCommandBuffer cmd, uint32_t frameIndex, const RgDrawFrameRadialBlurEffectParams *params, float currentTime)
    {
        if (params == nullptr)
        {
            return false;
        }
        
        if (params->beginNow)
        {
            push.beginTime = currentTime;
            push.endTime = currentTime + params->duration;
            push.transitionDuration = std::max(0.0f, params->transitionDuration);
        }

        if (push.beginTime >= push.endTime || currentTime >= push.endTime)
        {
            return false;
        }

        return true;
    }

    FramebufferImageIndex Apply(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const std::shared_ptr<Framebuffers> &framebuffers, const std::shared_ptr<const GlobalUniform> &uniform,
        uint32_t width, uint32_t height, FramebufferImageIndex inputFramebuf)
    {
        VkDescriptorSet descSets[] =
        {
            framebuffers->GetDescSet(frameIndex),
            uniform->GetDescSet(frameIndex),
        };

        return Dispatch(cmd, frameIndex, framebuffers, width, height, inputFramebuf, descSets);
    }

protected:
    const char *GetShaderName() override
    {
        return "EffectRadialBlur";
    }

    bool GetPushConstData(uint8_t(&pData)[128], uint32_t *pDataSize) const override
    {
        memcpy(pData, &push, sizeof(push));
        *pDataSize = sizeof(push);
        return true;
    }

private:
    PushConst push;
};

}
