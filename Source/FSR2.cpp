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

#include "FSR2.h"

#include <ffx_fsr2.h>
#include <vk/ffx_fsr2_vk.h>

#include "RenderResolutionHelper.h"
#include "RgException.h"

namespace
{
    void CheckError(FfxErrorCode r)
    {
        if (r != FFX_OK)
        {
            throw RTGL1::RgException(RG_GRAPHICS_API_ERROR, "Can't initialize FSR2");
        }
    }
}

RTGL1::FSR2::FSR2(VkDevice _device, VkPhysicalDevice _physDevice)
    : device(_device)
    , physDevice(_physDevice)
    , context(std::make_unique<std::optional<FfxFsr2Context>>())
{
}

RTGL1::FSR2::~FSR2()
{
    if (context->has_value())
    {
        ffxFsr2ContextDestroy(&context->value());
    }
}

void RTGL1::FSR2::OnFramebuffersSizeChange(const ResolutionState &resolutionState)
{
    if (context->has_value())
    {
        ffxFsr2ContextDestroy(&context->value());
        context->reset();
    }
    context->emplace();

    FfxErrorCode r;

    FfxFsr2ContextDescription contextDesc =
    {
        .flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE, // | FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE
        .maxRenderSize = { resolutionState.renderWidth, resolutionState.renderHeight },
        .displaySize = { resolutionState.upscaledWidth, resolutionState.upscaledHeight },
        .callbacks = {},
        .device = device
    };

    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeVK(physDevice);
    void *scratchBuffer = malloc(scratchBufferSize);

    r = ffxFsr2GetInterfaceVK(&contextDesc.callbacks, scratchBuffer, scratchBufferSize, physDevice, vkGetDeviceProcAddr);
    CheckError(r);

    r = ffxFsr2ContextCreate(&context->value(), &contextDesc);
    CheckError(r);
}

namespace 
{
    constexpr RTGL1::FramebufferImageIndex OUTPUT_IMAGE_INDEX = RTGL1::FB_IMAGE_INDEX_UPSCALED_PONG;

    FfxResource ToFSRResource(
        RTGL1::FramebufferImageIndex fbImage, uint32_t frameIndex,
        FfxFsr2Context *pCtx,
        const RTGL1::Framebuffers &framebuffers,
        const RTGL1::ResolutionState &resolutionState)
    {
        auto [image, view, format, sz] = framebuffers.GetImageHandles(fbImage, frameIndex, resolutionState);

        return ffxGetTextureResourceVK(
            pCtx, image, view, sz.width, sz.height, format, nullptr,
            fbImage == OUTPUT_IMAGE_INDEX ? FFX_RESOURCE_STATE_UNORDERED_ACCESS : FFX_RESOURCE_STATE_COMPUTE_READ);
    }

    template<size_t N>
    void InsertBarriers(
        VkCommandBuffer cmd, uint32_t frameIndex, RTGL1::Framebuffers &framebuffers, 
        const RTGL1::FramebufferImageIndex (&inputsAndOutput)[N],
        bool isBackwards)
    {
        assert(std::find(std::begin(inputsAndOutput), std::end(inputsAndOutput), OUTPUT_IMAGE_INDEX) != std::end(inputsAndOutput));

        VkImageMemoryBarrier2 barriers[N];

        for (size_t i = 0; i < N; i++)
        {
            auto &b = barriers[i];
            b = {};

            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            b.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            b.dstAccessMask = inputsAndOutput[i] == OUTPUT_IMAGE_INDEX ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_SHADER_READ_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            b.newLayout = inputsAndOutput[i] == OUTPUT_IMAGE_INDEX ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = framebuffers.GetImage(inputsAndOutput[i], frameIndex);
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;

            if (isBackwards)
            {
                std::swap(b.srcStageMask, b.dstStageMask);
                std::swap(b.srcAccessMask, b.dstAccessMask);
                std::swap(b.oldLayout, b.newLayout);
                std::swap(b.srcQueueFamilyIndex, b.dstQueueFamilyIndex);
            }
        }

        VkDependencyInfoKHR dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(std::size(barriers));
        dependencyInfo.pImageMemoryBarriers = barriers;

        RTGL1::svkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
    }
}

RTGL1::FramebufferImageIndex RTGL1::FSR2::Apply(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<Framebuffers>& framebuffers, 
    const RenderResolutionHelper& renderResolution,
    RgFloat2D jitterOffset,
    float timeDelta, 
    float nearPlane,
    float farPlane,
    float fovVerticalRad)
{
    assert(nearPlane > 0.0f && nearPlane < farPlane);
    
    using FI = RTGL1::FramebufferImageIndex;

    FI rs[] =
    {
        FI::FB_IMAGE_INDEX_FINAL,
        FI::FB_IMAGE_INDEX_DEPTH_NDC,
        FI::FB_IMAGE_INDEX_MOTION_DLSS,
        OUTPUT_IMAGE_INDEX
    };
    InsertBarriers(cmd, frameIndex, *framebuffers, rs, false);

    FfxFsr2Context *pCtx = &context->value();

    FfxFsr2DispatchDescription info = {};
    info.commandList = ffxGetCommandListVK(cmd);
    info.color = ToFSRResource(FI::FB_IMAGE_INDEX_FINAL, frameIndex, pCtx, *framebuffers, renderResolution.GetResolutionState());
    info.depth = ToFSRResource(FI::FB_IMAGE_INDEX_DEPTH_NDC, frameIndex, pCtx, *framebuffers, renderResolution.GetResolutionState());
    info.motionVectors = ToFSRResource(FI::FB_IMAGE_INDEX_MOTION_DLSS, frameIndex, pCtx, *framebuffers, renderResolution.GetResolutionState());
    info.exposure = {};
    info.reactive = {};
    info.transparencyAndComposition = {};
    info.output = ToFSRResource(OUTPUT_IMAGE_INDEX, frameIndex, pCtx, *framebuffers, renderResolution.GetResolutionState());
    info.jitterOffset.x = -jitterOffset.data[0];
    info.jitterOffset.y = -jitterOffset.data[1];
    info.motionVectorScale.x = static_cast<float>(renderResolution.GetResolutionState().renderWidth);
    info.motionVectorScale.y = static_cast<float>(renderResolution.GetResolutionState().renderHeight);
    info.reset = false;
    info.enableSharpening = renderResolution.IsCASInsideFSR2();
    info.sharpness = renderResolution.GetSharpeningIntensity();
    info.frameTimeDelta = timeDelta;
    info.preExposure = 1.0f;
    info.renderSize.width = renderResolution.GetResolutionState().renderWidth;
    info.renderSize.height = renderResolution.GetResolutionState().renderHeight;
    info.cameraFar = farPlane;
    info.cameraNear = nearPlane;
    info.cameraFovAngleVertical = fovVerticalRad;

    FfxErrorCode r = ffxFsr2ContextDispatch(pCtx, &info);
    CheckError(r);

    InsertBarriers(cmd, frameIndex, *framebuffers, rs, true);

    return OUTPUT_IMAGE_INDEX;
}

RgFloat2D RTGL1::FSR2::GetJitter(const ResolutionState& resolutionState, uint32_t frameId)
{
    const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(resolutionState.renderWidth, resolutionState.upscaledWidth);

    RgFloat2D jitter = {};
    FfxErrorCode r = ffxFsr2GetJitterOffset(&jitter.data[0], &jitter.data[1], frameId, jitterPhaseCount);
    assert(r == FFX_OK);

    return jitter;
}
