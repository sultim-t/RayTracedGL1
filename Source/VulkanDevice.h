#pragma once

#include <RTGL1/RTGL1.h>

#include <memory>

#include "CommandBufferManager.h"
#include "Common.h"
#include "PhysicalDevice.h"
#include "Scene.h"
#include "Swapchain.h"
#include "Queues.h"
#include "GlobalUniform.h"
#include "PathTracer.h"
#include "Rasterizer.h"
#include "BasicStorageImage.h"

class VulkanDevice
{
public:
    explicit VulkanDevice(const RgInstanceCreateInfo *info);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice& other) = delete;
    VulkanDevice(VulkanDevice&& other) noexcept = delete;
    VulkanDevice& operator=(const VulkanDevice& other) = delete;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept = delete;

    RgResult UploadGeometry(const RgGeometryUploadInfo *uploadInfo, RgGeometry *result);
    RgResult UpdateGeometryTransform(const RgUpdateTransformInfo *updateInfo);

    RgResult UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *uploadInfo);

    RgResult SubmitStaticGeometries();
    RgResult StartNewStaticScene();

    // TODO: empty (white) texture 1x1 with index 0 (RG_NO_TEXTURE)

    RgResult StartFrame(uint32_t surfaceWidth, uint32_t surfaceHeight);
    RgResult DrawFrame(const RgDrawFrameInfo *frameInfo);

private:
    void CreateInstance(const char **ppWindowExtensions, uint32_t extensionCount);
    void CreateDevice();
    void CreateSyncPrimitives();
    static VkSurfaceKHR GetSurfaceFromUser(VkInstance instance, const RgInstanceCreateInfo &info);

    void DestroyInstance();
    void DestroyDevice();
    void DestroySyncPrimitives();

    void FillUniform(ShGlobalUniform *gu, const RgDrawFrameInfo *frameInfo);

    VkCommandBuffer BeginFrame(uint32_t surfaceWidth, uint32_t surfaceHeight);
    void Render(VkCommandBuffer cmd, uint32_t renderWidth, uint32_t renderHeight);
    void EndFrame(VkCommandBuffer cmd);

private:
    VkInstance          instance;
    VkDevice            device;
    VkSurfaceKHR        surface;

    // [0..MAX_FRAMES_IN_FLIGHT-1]
    uint32_t            currentFrameIndex;
    VkCommandBuffer     currentFrameCmd;

    VkFence             frameFences[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];

    std::shared_ptr<PhysicalDevice>         physDevice;
    std::shared_ptr<Queues>                 queues;
    std::shared_ptr<Swapchain>              swapchain;

    std::shared_ptr<CommandBufferManager>   cmdManager;

    // TODO: this storage image is only for debugging in the beginning
    std::shared_ptr<BasicStorageImage>      storageImage;

    std::shared_ptr<GlobalUniform>          uniform;
    std::shared_ptr<Scene>                  scene;

    std::shared_ptr<ShaderManager>          shaderManager;
    std::shared_ptr<RayTracingPipeline>     rtPipeline;
    std::shared_ptr<PathTracer>             pathTracer;
    std::shared_ptr<Rasterizer>             rasterizer;

    bool                                    enableValidationLayer;
    VkDebugUtilsMessengerEXT                debugMessenger;
    PFN_rgPrint                             debugPrint;

    VertexBufferProperties                  vbProperties;
};
