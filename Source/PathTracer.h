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

#include "Common.h"
#include "RayTracingPipeline.h"
#include "GlobalUniform.h"
#include "Framebuffers.h"
#include "RenderCubemap.h"

namespace RTGL1
{

class PathTracer
{
public:
    struct TraceParams
    {
    private:
        friend class PathTracer;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        std::shared_ptr<Framebuffers> framebuffers;
    };

public:
    explicit PathTracer(VkDevice device, std::shared_ptr<RayTracingPipeline> rtPipeline);
    ~PathTracer() = default;

    PathTracer(const PathTracer& other) = delete;
    PathTracer(PathTracer&& other) noexcept = delete;
    PathTracer& operator=(const PathTracer& other) = delete;
    PathTracer& operator=(PathTracer&& other) noexcept = delete;

    TraceParams Bind(
        VkCommandBuffer cmd, uint32_t frameIndex,
        uint32_t width, uint32_t height,
        const std::shared_ptr<Scene> &scene,
        const std::shared_ptr<GlobalUniform> &uniform,
        const std::shared_ptr<TextureManager> &textureManager,
        const std::shared_ptr<Framebuffers> &framebuffers, 
        const std::shared_ptr<BlueNoise> &blueNoise,
        const std::shared_ptr<CubemapManager> &cubemapManager,
        const std::shared_ptr<RenderCubemap> &renderCubemap);
    
    void TracePrimaryRays(const TraceParams &params);
    void TraceReflectionRefractionRays(const TraceParams &params);
    void TraceDirectllumination(const TraceParams &params);
    void CalculateGradientsSamples(const TraceParams &params);
    void TraceIndirectllumination(const TraceParams &params);

private:
    void TraceRays(
        VkCommandBuffer cmd,
        uint32_t sbtRayGenIndex,
        uint32_t width, uint32_t height, uint32_t depth = 1);

private:
    std::shared_ptr<RayTracingPipeline> rtPipeline;
};

}