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
#include "VertexBufferManager.h"

class VulkanDevice
{
public:
    VulkanDevice(const RgInstanceCreateInfo *info);
    ~VulkanDevice();

    RgResult CreateGeometry(const RgGeometryCreateInfo *createInfo,
                            RgGeometry *result);
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

private:
    VkInstance          instance;
    VkDevice            device;
    VkSurfaceKHR        surface;

    // [0..MAX_FRAMES_IN_FLIGHT-1]
    uint32_t            currentFrameIndex;

    VkFence             stagingStaticGeomFence;
    VkFence             frameFences[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];

    std::shared_ptr<PhysicalDevice>         physDevice;
    std::shared_ptr<Queues>                 queues;
    std::shared_ptr<Swapchain>              swapchain;

    std::shared_ptr<CommandBufferManager>   cmdBufferManager;

    std::shared_ptr<GlobalUniform>          uniformBuffers;
    std::shared_ptr<VertexBufferManager>    vertexBufferManager;

    bool                                    enableValidationLayer;
    VkDebugUtilsMessengerEXT                debugMessenger;
    PFN_rgPrint                             debugPrint;
};
