#pragma once

#include "Common.h"
#include "RayTracingPipeline.h"
#include "GlobalUniform.h"

class PathTracer
{
public:
    explicit PathTracer(VkDevice device, std::shared_ptr<RayTracingPipeline> rtPipeline);
    ~PathTracer();

    PathTracer(const PathTracer& other) = delete;
    PathTracer(PathTracer&& other) noexcept = delete;
    PathTracer& operator=(const PathTracer& other) = delete;
    PathTracer& operator=(PathTracer&& other) noexcept = delete;

    // TODO: remove imagesDescSet
    void Trace(
        VkCommandBuffer cmd, uint32_t frameIndex,
        uint32_t width, uint32_t height,
        const std::shared_ptr<ASManager> &asManager,
        const std::shared_ptr<GlobalUniform> &uniform,
        VkDescriptorSet imagesDescSet);

private:
    VkDevice device;
    std::shared_ptr<RayTracingPipeline> rtPipeline;
};