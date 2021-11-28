// Copyright (c) 2020-2021 Sultim Tsyrendashiev
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

#include <cassert>

#include "DLSS.h"
#include "RgException.h"

namespace RTGL1
{

class RenderResolutionHelper
{
public:
    RenderResolutionHelper() = default;
    ~RenderResolutionHelper() = default;

    void Setup(const RgDrawFrameRenderResolutionParams *pParams, 
               uint32_t fullWidth, uint32_t fullHeight,
               const std::shared_ptr<DLSS> &dlss)
    {   
        renderWidth = fullWidth;
        renderHeight = fullHeight;

        upscaledWidth = fullWidth;
        upscaledHeight = fullHeight;

        dlssSharpness = 0;


        if (pParams == nullptr)
        {
            upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_LINEAR;
            sharpenTechnique = RG_RENDER_SHARPEN_TECHNIQUE_NONE;
            resolutionMode = RG_RENDER_RESOLUTION_MODE_CUSTOM;

            return;
        }


        upscaleTechnique = pParams->upscaleTechnique;
        sharpenTechnique = pParams->sharpenTechnique;
        resolutionMode   = pParams->resolutionMode;

        // check for correct values
        {
            switch (upscaleTechnique)
            {
                case RG_RENDER_UPSCALE_TECHNIQUE_LINEAR:
                case RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR:
                case RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS:
                    break;
                default:
                    throw RgException(RG_WRONG_ARGUMENT, "RgDrawFrameRenderResolutionParams::upscaleTechnique is incorrect");
            }
            switch (sharpenTechnique)
            {
                case RG_RENDER_SHARPEN_TECHNIQUE_NONE:
                case RG_RENDER_SHARPEN_TECHNIQUE_NAIVE:
                case RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS:
                    break;
                default:
                    throw RgException(RG_WRONG_ARGUMENT, "RgDrawFrameRenderResolutionParams::sharpenTechnique is incorrect");
            }
            switch (resolutionMode)
            {
                case RG_RENDER_RESOLUTION_MODE_CUSTOM:
                case RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE:
                case RG_RENDER_RESOLUTION_MODE_PERFORMANCE:
                case RG_RENDER_RESOLUTION_MODE_BALANCED:
                case RG_RENDER_RESOLUTION_MODE_QUALITY:
                case RG_RENDER_RESOLUTION_MODE_ULTRA_QUALITY:
                    break;
                default:
                    throw RgException(RG_WRONG_ARGUMENT, "RgDrawFrameRenderResolutionParams::resolutionMode is incorrect");
            }
        }


        if (resolutionMode == RG_RENDER_RESOLUTION_MODE_CUSTOM)
        {
            renderWidth  = pParams->renderSize.width;
            renderHeight = pParams->renderSize.height;

            return;
        }


        if (upscaleTechnique == RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR)
        {
            if (resolutionMode == RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE)
            {
                resolutionMode = RG_RENDER_RESOLUTION_MODE_PERFORMANCE;
            }

            float mult = 1.0f;

            switch (resolutionMode)
            {
                case RG_RENDER_RESOLUTION_MODE_PERFORMANCE:     mult = 0.5f;  break;
                case RG_RENDER_RESOLUTION_MODE_BALANCED:        mult = 0.59f; break;
                case RG_RENDER_RESOLUTION_MODE_QUALITY:         mult = 0.67f; break;
                case RG_RENDER_RESOLUTION_MODE_ULTRA_QUALITY:   mult = 0.77f; break;
                default: assert(0); break;
            }

            renderWidth  = mult * fullWidth;
            renderHeight = mult * fullHeight;
        }
        else if (upscaleTechnique == RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS)
        {
            dlss->GetOptimalSettings(fullWidth, fullHeight, resolutionMode,
                                     &renderWidth, &renderHeight, &dlssSharpness);
        }
    }

    uint32_t Width()            const { return renderWidth; }
    uint32_t Height()           const { return renderHeight; }

    uint32_t UpscaledWidth()    const { return upscaledWidth; }
    uint32_t UpscaledHeight()   const { return upscaledHeight; }
    
    bool IsAmdFsrEnabled()      const { return upscaleTechnique == RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR; }
    bool IsNvDlssEnabled()      const { return upscaleTechnique == RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS; }
    float GetAmdFsrSharpness()  const { return 0.0f; }          // 0.0 - max, 1.0 - min
    float GetNvDlssSharpness()  const { return dlssSharpness; } 

    // For the additional sharpening pass
    RgRenderSharpenTechnique GetSharpeningTechnique() const { return sharpenTechnique; }
    bool                     IsSharpeningEnabled()    const { return sharpenTechnique != RG_RENDER_SHARPEN_TECHNIQUE_NONE; }
    float                    GetSharpeningIntensity() const { return 1.0f; }

    // RgRenderResolutionMode   GetResolutionMode()      const { return resolutionMode; }

private:
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;

    uint32_t upscaledWidth = 0;
    uint32_t upscaledHeight = 0;

    RgRenderUpscaleTechnique    upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_LINEAR;
    RgRenderSharpenTechnique    sharpenTechnique = RG_RENDER_SHARPEN_TECHNIQUE_NONE;
    RgRenderResolutionMode      resolutionMode   = RG_RENDER_RESOLUTION_MODE_CUSTOM;

    float dlssSharpness = 0.0f;
};

}