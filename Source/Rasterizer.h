#pragma once

#include <vector>

#include "Common.h"
#include "RTGL1/RTGL1.h"
#include "Buffer.h"
#include "ShaderManager.h"
#include "ISwapchainDependency.h"
#include "RasterizedDataCollector.h"

// This class provides rasterization functionality
class Rasterizer : public ISwapchainDependency
{
public:
    explicit Rasterizer(
        VkDevice device, 
        const std::shared_ptr<PhysicalDevice> &physDevice,
        const std::shared_ptr<ShaderManager> &shaderManager,
        VkDescriptorSetLayout texturesDescLayout,
        VkFormat surfaceFormat,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~Rasterizer() override;

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void Upload(const RgRasterizedGeometryUploadInfo &uploadInfo, uint32_t frameIndex);
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex, VkDescriptorSet texturesDescSet);

    void OnSwapchainCreate(const Swapchain *pSwapchain) override;
    void OnSwapchainDestroy() override;

private:
    void CreateRenderPass(VkFormat surfaceFormat);
    void CreatePipelineCache();
    void CreatePipelineLayout(VkDescriptorSetLayout texturesDescLayout);
    void CreatePipeline(const std::shared_ptr<ShaderManager> &shaderManager);
    void CreateFramebuffers(uint32_t width, uint32_t height, const VkImageView *pFrameAttchs, uint32_t count);
    void DestroyFramebuffers();
    //void CreateDescriptors();

private:
    VkDevice device;

    VkRenderPass        renderPass;
    VkPipelineLayout    pipelineLayout;
    VkPipelineCache     pipelineCache;
    VkPipeline          pipeline;

    VkRect2D  curRenderArea;
    VkViewport curViewport;
    std::vector<VkFramebuffer> framebuffers;

    std::shared_ptr<RasterizedDataCollector> collectors[MAX_FRAMES_IN_FLIGHT];

    //VkDescriptorSetLayout descLayout;
    //VkDescriptorPool descPool;
    //VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];
};