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

#include "VulkanDevice.h"

#include <algorithm>

#include "Matrix.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

VulkanDevice::VulkanDevice(const RgInstanceCreateInfo *info) :
    instance(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    currentFrameIndex(MAX_FRAMES_IN_FLIGHT - 1),
    currentFrameCmd(VK_NULL_HANDLE),
    frameId(1),
    enableValidationLayer(info->enableValidationLayer == RG_TRUE),
    debugMessenger(VK_NULL_HANDLE),
    debugPrint(info->pfnDebugPrint),
    previousFrameTime(-1.0 / 60.0),
    currentFrameTime(0)
{
    vbProperties.vertexArrayOfStructs = info->vertexArrayOfStructs == RG_TRUE;
    vbProperties.positionStride = info->vertexPositionStride;
    vbProperties.normalStride = info->vertexNormalStride;
    vbProperties.texCoordStride = info->vertexTexCoordStride;
    vbProperties.colorStride = info->vertexColorStride;

    // init vulkan instance 
    CreateInstance(info->ppWindowExtensions, info->windowExtensionCount);


    // create VkSurfaceKHR using user's function
    surface = GetSurfaceFromUser(instance, *info);


    // create selected physical device
    physDevice          = std::make_shared<PhysicalDevice>(instance, info->physicalDeviceIndex);
    queues              = std::make_shared<Queues>(physDevice->Get(), surface);

    // create vulkan device and set extension function pointers
    CreateDevice();

    CreateSyncPrimitives();

    // set device
    queues->SetDevice(device);


    memAllocator        = std::make_shared<MemoryAllocator>(instance, device, physDevice);

    cmdManager          = std::make_shared<CommandBufferManager>(device, queues);

    uniform             = std::make_shared<GlobalUniform>(device, memAllocator);

    swapchain           = std::make_shared<Swapchain>(device, surface, physDevice, cmdManager);

    samplerManager      = std::make_shared<SamplerManager>(device);

    framebuffers        = std::make_shared<Framebuffers>(
        device,
        memAllocator, 
        cmdManager, 
        samplerManager);

    blueNoise           = std::make_shared<BlueNoise>(
        device,
        "../../../BlueNoise/Data/64_64/",
        memAllocator,
        cmdManager, 
        samplerManager);

    textureManager      = std::make_shared<TextureManager>(
        device, 
        memAllocator,
        samplerManager, 
        cmdManager,
        info->overridenTexturesFolderPath,
        info->overrideAlbedoAlphaTexturePostfix, 
        info->overrideNormalMetallicTexturePostfix, 
        info->overrideEmissionRoughnessTexturePostfix);

    auto asManager      = std::make_shared<ASManager>(
        device, 
        memAllocator, 
        cmdManager, 
        textureManager, 
        vbProperties);
    
    auto lightManager   = std::make_shared<LightManager>(device, memAllocator);

    scene               = std::make_shared<Scene>(asManager, lightManager);

    shaderManager       = std::make_shared<ShaderManager>(device);
   
    rtPipeline          = std::make_shared<RayTracingPipeline>(
        device, 
        physDevice, 
        memAllocator, 
        shaderManager,
        scene,
        uniform, 
        textureManager,
        framebuffers, 
        blueNoise);

    pathTracer          = std::make_shared<PathTracer>(device, rtPipeline);

    rasterizer          = std::make_shared<Rasterizer>(
        device,
        memAllocator,
        shaderManager,
        textureManager, 
        swapchain->GetSurfaceFormat(),
        info->rasterizedMaxVertexCount, 
        info->rasterizedMaxIndexCount);

    tonemapping = std::make_shared<Tonemapping>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        memAllocator);

    imageComposition    = std::make_shared<ImageComposition>(
        device, 
        framebuffers, 
        shaderManager, 
        uniform, 
        tonemapping);


    swapchain->Subscribe(framebuffers);
    swapchain->Subscribe(rasterizer);
}

VulkanDevice::~VulkanDevice()
{
    vkDeviceWaitIdle(device);

    physDevice.reset();
    queues.reset();
    swapchain.reset();
    cmdManager.reset();
    framebuffers.reset();
    tonemapping.reset();
    imageComposition.reset();
    uniform.reset();
    scene.reset();
    shaderManager.reset();
    rtPipeline.reset();
    pathTracer.reset();
    rasterizer.reset();
    samplerManager.reset();
    blueNoise.reset();
    textureManager.reset();
    memAllocator.reset();

    vkDestroySurfaceKHR(instance, surface, nullptr);
    DestroySyncPrimitives();

    DestroyDevice();
    DestroyInstance();
}

VkCommandBuffer VulkanDevice::BeginFrame(uint32_t surfaceWidth, uint32_t surfaceHeight, bool vsync)
{
    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    uint32_t frameIndex = currentFrameIndex;

    // wait for previous cmd with the same frame index
    Utils::WaitAndResetFence(device, frameFences[frameIndex]);

    swapchain->RequestNewSize(surfaceWidth, surfaceHeight);
    swapchain->RequestVsync(vsync);
    swapchain->AcquireImage(imageAvailableSemaphores[frameIndex]);

    // reset cmds for current frame index
    cmdManager->PrepareForFrame(frameIndex);

    // destroy staging buffers that were created MAX_FRAMES_IN_FLIGHT ago
    textureManager->PrepareForFrame(frameIndex);

    // start dynamic geometry recording to current frame
    scene->PrepareForFrame(frameIndex);

    return cmdManager->StartGraphicsCmd();
}

void VulkanDevice::FillUniform(ShGlobalUniform *gu, const RgDrawFrameInfo *frameInfo) const
{
    memcpy(gu->viewPrev, gu->view, 16 * sizeof(float));
    memcpy(gu->projectionPrev, gu->projection, 16 * sizeof(float));

    memcpy(gu->view, frameInfo->view, 16 * sizeof(float));
    memcpy(gu->projection, frameInfo->projection, 16 * sizeof(float));

    Matrix::Inverse(gu->invView, frameInfo->view);
    Matrix::Inverse(gu->invProjection, frameInfo->projection);

    // to remove additional division by 4 bytes in shaders
    gu->positionsStride = vbProperties.positionStride / 4;
    gu->normalsStride = vbProperties.normalStride / 4;
    gu->texCoordsStride = vbProperties.texCoordStride / 4;

    gu->renderWidth = frameInfo->renderWidth;
    gu->renderHeight = frameInfo->renderHeight;
    gu->frameId = frameId;

    gu->timeDelta = std::max<double>(currentFrameTime - previousFrameTime, 0.001);
    
    if (frameInfo->overrideTonemappingParams)
    {
        gu->minLogLuminance = frameInfo->minLogLuminance;
        gu->maxLogLuminance = frameInfo->maxLogLuminance;
        gu->luminanceWhitePoint = frameInfo->luminanceWhitePoint;
    }
    else
    {
        gu->minLogLuminance = -2.0f;
        gu->maxLogLuminance = 10.0f;
        gu->luminanceWhitePoint = 1.5f;
    }

    gu->stopEyeAdaptation = frameInfo->disableEyeAdaptation;

    gu->lightSourceCountSpherical = scene->GetLightManager()->GetSphericalLightCount();
    gu->lightSourceCountDirectional = scene->GetLightManager()->GetDirectionalLightCount();
}

void VulkanDevice::Render(VkCommandBuffer cmd, uint32_t renderWidth, uint32_t renderHeight)
{
    uint32_t frameIndex = currentFrameIndex;

    textureManager->SubmitDescriptors(frameIndex);

    // submit geometry
    bool sceneNotEmpty = scene->SubmitForFrame(cmd, frameIndex, uniform);

    // update uniform data
    uniform->Upload(frameIndex);

    if (sceneNotEmpty)
    {
        pathTracer->Trace(
            cmd, frameIndex, renderWidth, renderHeight,
            scene, uniform, textureManager, framebuffers, blueNoise);
    }

    // tonemapping
    tonemapping->Tonemap(cmd, frameIndex, uniform);

    // final image composition
    imageComposition->Compose(cmd, frameIndex, uniform, tonemapping);

    framebuffers->Barrier(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);

    // blit result image to present on a surface
    framebuffers->PresentToSwapchain(
        cmd, frameIndex, swapchain, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL,
        renderWidth, renderHeight, VK_IMAGE_LAYOUT_GENERAL);

    // draw rasterized geometry in swapchain's framebuffer
    rasterizer->Draw(cmd, frameIndex);
}

void VulkanDevice::EndFrame(VkCommandBuffer cmd)
{
    uint32_t frameIndex = currentFrameIndex;

    // submit command buffer, but wait until presentation engine has completed using image
    cmdManager->Submit(
        cmd, 
        imageAvailableSemaphores[frameIndex],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        renderFinishedSemaphores[frameIndex],
        frameFences[frameIndex]);

    // present on a surface when rendering will be finished
    swapchain->Present(queues, renderFinishedSemaphores[frameIndex]);

    frameId++;
}



#pragma region RTGL1 interface implementation

RgResult VulkanDevice::StartFrame(uint32_t surfaceWidth, uint32_t surfaceHeight, bool vsync)
{
    if (currentFrameCmd != VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_ENDED;
    }

    currentFrameCmd = BeginFrame(surfaceWidth, surfaceHeight, vsync);
    return RG_SUCCESS;
}

RgResult VulkanDevice::DrawFrame(const RgDrawFrameInfo *frameInfo)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    previousFrameTime = currentFrameTime;
    currentFrameTime = frameInfo->currentTime;

    FillUniform(uniform->GetData(), frameInfo);

    Render(currentFrameCmd, frameInfo->renderWidth, frameInfo->renderHeight);
    EndFrame(currentFrameCmd);

    currentFrameCmd = VK_NULL_HANDLE;

    return RG_SUCCESS;
}


RgResult VulkanDevice::UploadGeometry(const RgGeometryUploadInfo *uploadInfo, RgGeometry *result)
{
    if (uploadInfo == nullptr || uploadInfo->vertexCount == 0)
    {
        return RG_WRONG_ARGUMENT;
    }

    uint32_t geomId = scene->Upload(currentFrameIndex, *uploadInfo);

    if (result!= nullptr)
    {
        *result = static_cast<RgGeometry>(geomId);
    }

    if (geomId == UINT32_MAX)
    {
        return RG_ERROR;
    }

    return RG_SUCCESS;
}

RgResult VulkanDevice::UpdateGeometryTransform(const RgUpdateTransformInfo *updateInfo)
{
    if (updateInfo == nullptr)
    {
        return RG_WRONG_ARGUMENT;
    }

    uint32_t geomId = static_cast<uint32_t>(updateInfo->movableStaticGeom);

    bool b = scene->UpdateTransform(geomId, updateInfo->transform);

    return b ? RG_SUCCESS : RG_UPDATING_NOT_MOVABLE;
}

RgResult VulkanDevice::UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *uploadInfo)
{
    // move checks to to RTGL.cpp
    if (uploadInfo == nullptr || uploadInfo->vertexCount == 0)
    {
        return RG_WRONG_ARGUMENT;
    }

    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    rasterizer->Upload(*uploadInfo, currentFrameIndex);
    return RG_SUCCESS;
}

RgResult VulkanDevice::SubmitStaticGeometries()
{
    scene->SubmitStatic();
    return RG_SUCCESS;
}

RgResult VulkanDevice::StartNewStaticScene()
{
    scene->StartNewStatic();
    return RG_SUCCESS;
}

RgResult VulkanDevice::UploadLight(const RgDirectionalLightUploadInfo *lightInfo)
{
    scene->UploadLight(currentFrameIndex, *lightInfo);
    return RG_SUCCESS;
}

RgResult VulkanDevice::UploadLight(const RgSphericalLightUploadInfo *lightInfo)
{
    scene->UploadLight(currentFrameIndex, *lightInfo);
    return RG_SUCCESS;
}

RgResult VulkanDevice::CreateStaticMaterial(const RgStaticMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    *result = textureManager->CreateStaticMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
    return RG_SUCCESS;
}

RgResult VulkanDevice::CreateAnimatedMaterial(const RgAnimatedMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    *result = textureManager->CreateAnimatedMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
    return RG_SUCCESS;
}

RgResult VulkanDevice::ChangeAnimatedMaterialFrame(RgMaterial animatedMaterial, uint32_t frameIndex)
{
    bool wasChanged = textureManager->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
    return wasChanged ? RG_SUCCESS : RG_CANT_UPDATE_ANIMATED_MATERIAL;
}

RgResult VulkanDevice::CreateDynamicMaterial(const RgDynamicMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    *result = textureManager->CreateDynamicMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
    return RG_SUCCESS;
}

RgResult VulkanDevice::UpdateDynamicMaterial(const RgDynamicMaterialUpdateInfo *updateInfo)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        return RG_FRAME_WASNT_STARTED;
    }

    bool wasUpdated = textureManager->UpdateDynamicMaterial(currentFrameCmd, *updateInfo);
    return wasUpdated ? RG_SUCCESS : RG_CANT_UPDATE_DYNAMIC_MATERIAL;
}

RgResult VulkanDevice::DestroyMaterial(RgMaterial material)
{
    if (material == RG_NO_MATERIAL)
    {
        return RG_SUCCESS;
    }

    textureManager->DestroyMaterial(currentFrameIndex, material);
    return RG_SUCCESS;
}
#pragma endregion 



#pragma region init / destroy

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    if (pUserData == nullptr)
    {
        return VK_FALSE;
    }

    const char *msg;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        msg = "Vulkan::VERBOSE::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        msg = "Vulkan::INFO::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        msg = "Vulkan::WARNING::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        msg = "Vulkan::ERROR::[%d][%s]\n%s\n\n";
    }
    else
    {
        msg = "Vulkan::[%d][%s]\n%s\n\n";
    }

    char buf[1024];
    auto fnPrint = static_cast<PFN_rgPrint>(pUserData);

    snprintf(buf, 1024, msg, pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
    fnPrint(buf);

    return VK_FALSE;
}

void VulkanDevice::CreateInstance(const char **ppWindowExtensions, uint32_t extensionCount)
{
    std::vector<const char *> extensions;

    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    if (enableValidationLayer)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    for (uint32_t i = 0; i < extensionCount; i++)
    {
        extensions.push_back(ppWindowExtensions[i]);
    }

    VkApplicationInfo appInfo = {};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = "Raytracing test";

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    instanceInfo.enabledExtensionCount = extensions.size();

    const char *layerNames[2] =
    {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_monitor"
    };
    instanceInfo.ppEnabledLayerNames = layerNames;
    instanceInfo.enabledLayerCount = enableValidationLayer ? 2 : 0;

    VkResult r = vkCreateInstance(&instanceInfo, nullptr, &instance);
    VK_CHECKERROR(r);

    if (enableValidationLayer && debugPrint != nullptr)
    {
        InitInstanceExtensionFunctions_DebugUtils(instance);

        // init debug utils
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
        debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugMessengerInfo.pfnUserCallback = DebugMessengerCallback;
        debugMessengerInfo.pUserData = static_cast<void *>(debugPrint);

        r = svkCreateDebugUtilsMessengerEXT(instance, &debugMessengerInfo, nullptr, &debugMessenger);
        VK_CHECKERROR(r);
    }
}

void VulkanDevice::CreateDevice()
{
    VkPhysicalDeviceFeatures features = {};
    features.robustBufferAccess = 1;
    features.fullDrawIndexUint32 = 1;
    features.imageCubeArray = 1;
    features.independentBlend = 1;
    features.geometryShader = 1;
    features.tessellationShader = 1;
    features.sampleRateShading = 0;
    features.dualSrcBlend = 1;
    features.logicOp = 1;
    features.multiDrawIndirect = 1;
    features.drawIndirectFirstInstance = 1;
    features.depthClamp = 1;
    features.depthBiasClamp = 1;
    features.fillModeNonSolid = 0;
    features.depthBounds = 1;
    features.wideLines = 0;
    features.largePoints = 0;
    features.alphaToOne = 1;
    features.multiViewport = 0;
    features.samplerAnisotropy = 1;
    features.textureCompressionETC2 = 0;
    features.textureCompressionASTC_LDR = 0;
    features.textureCompressionBC = 0;
    features.occlusionQueryPrecise = 0;
    features.pipelineStatisticsQuery = 1;
    features.vertexPipelineStoresAndAtomics = 1;
    features.fragmentStoresAndAtomics = 1;
    features.shaderTessellationAndGeometryPointSize = 1;
    features.shaderImageGatherExtended = 1;
    features.shaderStorageImageExtendedFormats = 1;
    features.shaderStorageImageMultisample = 1;
    features.shaderStorageImageReadWithoutFormat = 1;
    features.shaderStorageImageWriteWithoutFormat = 1;
    features.shaderUniformBufferArrayDynamicIndexing = 1;
    features.shaderSampledImageArrayDynamicIndexing = 1;
    features.shaderStorageBufferArrayDynamicIndexing = 1;
    features.shaderStorageImageArrayDynamicIndexing = 1;
    features.shaderClipDistance = 1;
    features.shaderCullDistance = 1;
    features.shaderFloat64 = 1;
    features.shaderInt64 = 1;
    features.shaderInt16 = 1;
    features.shaderResourceResidency = 1;
    features.shaderResourceMinLod = 1;
    features.sparseBinding = 1;
    features.sparseResidencyBuffer = 1;
    features.sparseResidencyImage2D = 1;
    features.sparseResidencyImage3D = 1;
    features.sparseResidency2Samples = 1;
    features.sparseResidency4Samples = 1;
    features.sparseResidency8Samples = 1;
    features.sparseResidency16Samples = 1;
    features.sparseResidencyAliased = 1;
    features.variableMultisampleRate = 0;
    features.inheritedQueries = 1;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.runtimeDescriptorArray = 1;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = 1;
    indexingFeatures.shaderStorageBufferArrayNonUniformIndexing = 1;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures = {};
    bufferAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferAddressFeatures.pNext = &indexingFeatures;
    bufferAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &bufferAddressFeatures;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &rtPipelineFeatures;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &asFeatures;
    physicalDeviceFeatures2.features = features;

    std::vector<const char *> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    if (enableValidationLayer)
    {
        deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queues->GetDeviceQueueCreateInfos(queueCreateInfos);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;
    deviceCreateInfo.enabledExtensionCount = (uint32_t) deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult r = vkCreateDevice(physDevice->Get(), &deviceCreateInfo, nullptr, &device);
    VK_CHECKERROR(r);

    InitDeviceExtensionFunctions(device);

    if (enableValidationLayer)
    {
        InitDeviceExtensionFunctions_DebugUtils(device);
    }
}

void VulkanDevice::CreateSyncPrimitives()
{
    VkResult r;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        VK_CHECKERROR(r);

        r = vkCreateFence(device, &fenceInfo, nullptr, &frameFences[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, imageAvailableSemaphores[i], VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, "Image available semaphore");
        SET_DEBUG_NAME(device, renderFinishedSemaphores[i], VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, "Render finished semaphore");
        SET_DEBUG_NAME(device, frameFences[i], VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, "Frame fence");
    }
}

VkSurfaceKHR VulkanDevice::GetSurfaceFromUser(VkInstance instance, const RgInstanceCreateInfo &info)
{
    uint64_t instanceHandle = reinterpret_cast<uint64_t>(instance);
    uint64_t surfaceHandle = 0;
    info.pfnCreateSurface(instanceHandle, &surfaceHandle);

    return reinterpret_cast<VkSurfaceKHR>(surfaceHandle);
}

void VulkanDevice::DestroyInstance()
{
    if (debugMessenger != VK_NULL_HANDLE)
    {
        svkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

void VulkanDevice::DestroyDevice()
{
    vkDestroyDevice(device, nullptr);
}

void VulkanDevice::DestroySyncPrimitives()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);

        vkDestroyFence(device, frameFences[i], nullptr);
    }
}

#pragma endregion 
