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

namespace RTGL1
{

class PathTracer
{
public:
    explicit PathTracer(VkDevice device, std::shared_ptr<RayTracingPipeline> rtPipeline);
    ~PathTracer();

    PathTracer(const PathTracer& other) = delete;
    PathTracer(PathTracer&& other) noexcept = delete;
    PathTracer& operator=(const PathTracer& other) = delete;
    PathTracer& operator=(PathTracer&& other) noexcept = delete;

    void Trace(
        VkCommandBuffer cmd, uint32_t frameIndex,
        uint32_t width, uint32_t height,
        const std::shared_ptr<ASManager> &asManager,
        const std::shared_ptr<GlobalUniform> &uniform,
        const std::shared_ptr<TextureManager> &textureMgr,
        const std::shared_ptr<Framebuffers> &framebuffers, 
        const std::shared_ptr<BlueNoise> &blueNoise);

private:
    VkDevice device;
    std::shared_ptr<RayTracingPipeline> rtPipeline;
};

}