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

#include "PathTracer.h"
#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"

using namespace RTGL1;

PathTracer::PathTracer(VkDevice _device, std::shared_ptr<RayTracingPipeline> _rtPipeline)
    : device(_device), rtPipeline(std::move(_rtPipeline))
{}

PathTracer::~PathTracer()
{}

void PathTracer::Bind(
    VkCommandBuffer cmd, uint32_t frameIndex, 
    const std::shared_ptr<Scene> &scene, 
    const std::shared_ptr<GlobalUniform> &uniform,
    const std::shared_ptr<TextureManager> &textureMgr,
    const std::shared_ptr<Framebuffers> &framebuffers,
    const std::shared_ptr<BlueNoise> &blueNoise,
    const std::shared_ptr<CubemapManager> &cubemapMgr,
    const std::shared_ptr<RenderCubemap> &renderCubemap)
{
    rtPipeline->Bind(cmd);

    VkDescriptorSet sets[] = {
        // ray tracing acceleration structures
        scene->GetASManager()->GetTLASDescSet(frameIndex),
        // storage images
        framebuffers->GetDescSet(frameIndex),
        // uniform
        uniform->GetDescSet(frameIndex),
        // vertex data
        scene->GetASManager()->GetBuffersDescSet(frameIndex),
        // textures
        textureMgr->GetDescSet(frameIndex),
        // uniform random
        blueNoise->GetDescSet(),
        // light sources
        scene->GetLightManager()->GetDescSet(frameIndex),
        // cubemaps
        cubemapMgr->GetDescSet(frameIndex),
        // dynamic cubemaps
        renderCubemap->GetDescSet()
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipeline->GetLayout(),
                            0, setCount, sets,
                            0, nullptr);
}

void PathTracer::TracePrimaryRays(
    VkCommandBuffer cmd, uint32_t frameIndex, 
    uint32_t width, uint32_t height)
{
    CmdLabel label(cmd, "Primary rays");

    VkStridedDeviceAddressRegionKHR raygenEntry, missEntry, hitEntry, callableEntry;

    // primary
    rtPipeline->GetEntries(SBT_INDEX_RAYGEN_PRIMARY, raygenEntry, missEntry, hitEntry, callableEntry);

    svkCmdTraceRaysKHR(
        cmd,
        &raygenEntry, &missEntry, &hitEntry, &callableEntry,
        width, height, 1);
}

void RTGL1::PathTracer::PrepareForTracingIllumination(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers)
{
    typedef FramebufferImageIndex FI;

    FI fs[] =
    {
        FI::FB_IMAGE_INDEX_ALBEDO,
        FI::FB_IMAGE_INDEX_NORMAL,
        FI::FB_IMAGE_INDEX_NORMAL_GEOMETRY,
        FI::FB_IMAGE_INDEX_METALLIC_ROUGHNESS,
        FI::FB_IMAGE_INDEX_DEPTH,
        FI::FB_IMAGE_INDEX_RANDOM_SEED,
        FI::FB_IMAGE_INDEX_SURFACE_POSITION,
        FI::FB_IMAGE_INDEX_VIEW_DIRECTION,
        FI::FB_IMAGE_INDEX_GRADIENT_SAMPLES
    };

    framebuffers->BarrierMultiple(cmd, frameIndex, fs);
}

void PathTracer::TraceDirectllumination(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height)
{
    CmdLabel label(cmd, "Direct illumination");

    VkStridedDeviceAddressRegionKHR raygenEntry, missEntry, hitEntry, callableEntry;
    rtPipeline->GetEntries(SBT_INDEX_RAYGEN_DIRECT, raygenEntry, missEntry, hitEntry, callableEntry);

    svkCmdTraceRaysKHR(
        cmd,
        &raygenEntry, &missEntry, &hitEntry, &callableEntry,
        width, height, 1);
}

void PathTracer::TraceIndirectllumination(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height)
{
    CmdLabel label(cmd, "Indirect illumination");
    
    VkStridedDeviceAddressRegionKHR raygenEntry, missEntry, hitEntry, callableEntry;
    rtPipeline->GetEntries(SBT_INDEX_RAYGEN_INDIRECT, raygenEntry, missEntry, hitEntry, callableEntry);

    svkCmdTraceRaysKHR(
        cmd,
        &raygenEntry, &missEntry, &hitEntry, &callableEntry,
        width, height, 1);
}
