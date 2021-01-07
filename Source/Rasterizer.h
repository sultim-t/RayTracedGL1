#pragma once

#include "Common.h"
#include "RTGL1/RTGL1.h"
#include "Buffer.h"
#include "ShaderManager.h"
#include "ISwapchainDependency.h"

// This class provides rasterization functionality
class Rasterizer
{
public:
    explicit Rasterizer(
        VkDevice device, 
        const std::shared_ptr<PhysicalDevice> &physDevice,
        const std::shared_ptr<ShaderManager> &shaderManager,
        VkDescriptorSetLayout texturesDescLayout,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~Rasterizer();

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void Upload(const RgRasterizedGeometryUploadInfo &uploadInfo);
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex);

private:
    void CreateRenderPass();
    void CreatePipelineCache();
    void CreatePipelineLayout(VkDescriptorSetLayout texturesDescLayout);
    void CreatePipeline(const std::shared_ptr<ShaderManager> &shaderManager);
    //void CreateDescriptors();

private:
    VkDevice device;

    Buffer vertexBuffers[MAX_FRAMES_IN_FLIGHT];
    Buffer indexBuffers[MAX_FRAMES_IN_FLIGHT];

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipelineCache pipelineCache;
    VkPipeline pipeline;


    //VkDescriptorSetLayout descLayout;
    //VkDescriptorPool descPool;
    //VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];
};