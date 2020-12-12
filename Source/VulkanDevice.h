#pragma once

#include <memory>
#include <vector>
#include <RTGL1/RTGL1.h>

#include "CommandBufferManager.h"
#include "Common.h"
#include "PhysicalDevice.h"
#include "Scene.h"
#include "Swapchain.h"
#include "Queues.h"
#include "GlobalUniform.h"
#include "Rasterizer.h"

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

    // TODO: empty (white) texture 1x1 with index 0 (RG_NO_TEXTURE)

    RgResult DrawFrame(const RgDrawFrameInfo *frameInfo);

private:
    void CreateInstance(const char **ppWindowExtensions, uint32_t extensionCount);
    void CreateDevice();
    void CreateSyncPrimitives();

    void DestroyInstance();
    void DestroyDevice();
    void DestroySyncPrimitives();

    void BeginFrame();
    void TracePaths();
    void Rasterize();
    void EndFrame();

private:
    VkInstance          instance;
    VkDevice            device;
    VkSurfaceKHR        surface;

    // [0..MAX_FRAMES_IN_FLIGHT-1]
    uint32_t            currentFrameIndex;

    VkFence             frameFences[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];

    std::shared_ptr<PhysicalDevice>         physDevice;
    std::shared_ptr<Queues>                 queues;
    std::shared_ptr<Swapchain>              swapchain;

    std::shared_ptr<CommandBufferManager>   cmdBufferManager;

    std::shared_ptr<GlobalUniform>          uniformBuffers;
    std::shared_ptr<Scene>                  scene;
    std::shared_ptr<Rasterizer>             rasterizer;

    bool                                    enableValidationLayer;
    VkDebugUtilsMessengerEXT                debugMessenger;
    PFN_rgPrint                             debugPrint;
};
