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
        EffectBase(device),
        push{}
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

    bool Setup(VkCommandBuffer cmd, uint32_t frameIndex,
               const std::shared_ptr<Swapchain> &swapchain, const std::shared_ptr<Framebuffers> &framebuffers,
               const RgDrawFrameWipeEffectParams *params, float currentTime, uint32_t currentFrameId, uint32_t width, uint32_t height)
    {
        if (params == nullptr)
        {
            return false;
        }
        
        push.stripWidthInPixels = (uint32_t)(width * clamp(params->stripWidth, 0.0f, 1.0f));

        if (params->beginNow)
        {
            push.startFrameId = currentFrameId;
            push.beginTime = currentTime;
            push.endTime = currentTime + params->duration;
        }

        if (push.stripWidthInPixels == 0 || 
            push.beginTime >= push.endTime ||
            currentTime >= push.endTime)
        {
            return false;
        }

        if (params->beginNow)
        {
            uint32_t previousSwapchainIndex = Utils::GetPreviousByModulo(swapchain->GetCurrentImageIndex(), swapchain->GetImageCount());
            VkImage src = swapchain->GetImage(previousSwapchainIndex);

            VkImage dst = framebuffers->GetImage(FB_IMAGE_INDEX_WIPE_EFFECT_SOURCE, frameIndex);

            VkImageBlit region = {};

            region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.srcOffsets[0] = { 0, 0, 0 };
            region.srcOffsets[1] = { static_cast<int32_t>(swapchain->GetWidth()), static_cast<int32_t>(swapchain->GetHeight()), 1 };

            region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.dstOffsets[0] = { 0, 0, 0 };
            region.dstOffsets[1] = { static_cast<int32_t>(swapchain->GetWidth()), static_cast<int32_t>(swapchain->GetHeight()), 1 };

            Utils::BarrierImage(
                cmd, src,
                VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            Utils::BarrierImage(
                cmd, dst,
                VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdBlitImage(cmd,
                           src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region, VK_FILTER_NEAREST);

            Utils::BarrierImage(
                cmd, dst,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

            Utils::BarrierImage(
                cmd, src,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE_KHR,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
