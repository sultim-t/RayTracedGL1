#include "PathTracer.h"

PathTracer::PathTracer(VkDevice _device, std::shared_ptr<RayTracingPipeline> _rtPipeline)
    : device(_device), rtPipeline(_rtPipeline)
{}

PathTracer::~PathTracer()
{}

void PathTracer::Trace(
    VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height,
    const std::shared_ptr<ASManager> &asManager,
    const std::shared_ptr<GlobalUniform> &uniform)
{
    rtPipeline->Bind(cmd);

    VkDescriptorSet sets[] = {
        // ray tracing acceleration structures
        asManager->GetTLASDescSet(frameIndex),
        // images
        ,
        // uniform
        uniform->GetDescSet(frameIndex),
        // vertex data
        asManager->GetBuffersDescSet(frameIndex)
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipeline->GetLayout(),
                            0, setCount, sets,
                            0, nullptr);

    VkStridedDeviceAddressRegionKHR raygenEntry, missEntry, hitEntry, callableEntry;
    rtPipeline->GetEntries(raygenEntry, missEntry, hitEntry, callableEntry);

    svkCmdTraceRaysKHR(
        cmd,
        &raygenEntry, &missEntry, &hitEntry, &callableEntry,
        width, height, 1);
}
