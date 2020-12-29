#include "VulkanDevice.h"
#include "Utils.h"

VulkanDevice::VulkanDevice(const RgInstanceCreateInfo *info) :
    instance(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    currentFrameIndex(MAX_FRAMES_IN_FLIGHT - 1),
    currentFrameCmd(VK_NULL_HANDLE),
    enableValidationLayer(info->enableValidationLayer == RG_TRUE),
    debugPrint(info->pfnDebugPrint)
{
    // init vulkan instance 
    CreateInstance(info->ppWindowExtensions, info->windowExtensionCount);

    // create VkSurfaceKHR using user's function
    surface = GetSurfaceFromUser(instance, *info);

    // create selected physical device
    physDevice = std::make_shared<PhysicalDevice>(instance, info->physicalDeviceIndex);
    queues = std::make_shared<Queues>(physDevice->Get(), surface);

    // create vulkan device and set extension function pointers
    CreateDevice();
    InitDeviceExtensionFunctions(device);

    CreateSyncPrimitives();

    // set device
    physDevice->SetDevice(device);
    queues->SetDevice(device);

    cmdManager = std::make_shared<CommandBufferManager>(device, queues);
    uniform = std::make_shared<GlobalUniform>(device, physDevice);

    swapchain = std::make_shared<Swapchain>(device, surface, physDevice, cmdManager);

    auto asManager = std::make_shared<ASManager>(device, physDevice, cmdManager, *info);
    scene = std::make_shared<Scene>(asManager);

    shaderManager = std::make_shared<ShaderManager>(device);
    rtPipeline = std::make_shared<RayTracingPipeline>(device, physDevice, shaderManager, scene->GetASManager(), uniform);

    pathTracer = std::make_shared<PathTracer>(device, rtPipeline);

    // TODO: storage image
}

VulkanDevice::~VulkanDevice()
{
    DestroySyncPrimitives();
    DestroyDevice();
    DestroyInstance();
}

VkCommandBuffer VulkanDevice::BeginFrame(uint32_t surfaceWidth, uint32_t surfaceHeight)
{
    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    // wait for previous cmd with the same frame index
    Utils::WaitAndResetFence(device, frameFences[currentFrameIndex]);

    swapchain->RequestNewSize(surfaceWidth, surfaceHeight);
    swapchain->RequestVsync(true);
    swapchain->AcquireImage(imageAvailableSemaphores[currentFrameIndex]);

    // reset cmds for current frame index
    cmdManager->PrepareForFrame(currentFrameIndex);

    // start dynamic geometry recording to current frame
    scene->PrepareForFrame(currentFrameIndex);

    return cmdManager->StartGraphicsCmd();
}

void VulkanDevice::Render(VkCommandBuffer cmd, uint32_t renderWidth, uint32_t renderHeight)
{
    // submit geometry
    scene->SubmitForFrame(cmd, currentFrameIndex);

    // update uniform data
    uniform->Upload(currentFrameIndex);

    // trace paths and draw rasterized geometry
    pathTracer->Trace(cmd, currentFrameIndex, renderWidth, renderHeight, scene->GetASManager(), uniform);
    rasterizer->Draw(cmd);
}

void VulkanDevice::EndFrame(VkCommandBuffer cmd)
{
    // blit result image to present on a surface
    swapchain->BlitForPresent(cmd, , , , );

    // submit command buffer, but wait until presentation engine has completed using image
    cmdManager->Submit(
        cmd, 
        imageAvailableSemaphores[currentFrameIndex], 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        renderFinishedSemaphores[currentFrameIndex], 
        frameFences[currentFrameIndex]);

    // present on a surface when rendering will be finished
    swapchain->Present(queues, renderFinishedSemaphores[currentFrameIndex]);
}

RgResult VulkanDevice::StartFrame(uint32_t surfaceWidth, uint32_t surfaceHeight)
{
    currentFrameCmd = BeginFrame(surfaceWidth, surfaceHeight);
    return RG_SUCCESS;
}

// TODO: check all members of input structs

RgResult VulkanDevice::DrawFrame(const RgDrawFrameInfo *frameInfo)
{
    Render(currentFrameCmd, frameInfo->renderExtent.width, frameInfo->renderExtent.height);
    EndFrame(currentFrameCmd);

    currentFrameCmd = VK_NULL_HANDLE;

    return RG_SUCCESS;
}


RgResult VulkanDevice::UploadGeometry(const RgGeometryUploadInfo *uploadInfo, RgGeometry *result)
{
    if (uploadInfo == nullptr || uploadInfo->vertexCount == 0 || uploadInfo->indexCount == 0)
    {
        return RG_WRONG_ARGUMENT;
    }

    uint32_t geomId = scene->Upload(*uploadInfo);

    if (result!= nullptr)
    {
        *result = reinterpret_cast<RgGeometry>(geomId);
    }

    return RG_SUCCESS;
}

RgResult VulkanDevice::UpdateGeometryTransform(const RgUpdateTransformInfo *updateInfo)
{
    if (updateInfo == nullptr)
    {
        return RG_WRONG_ARGUMENT;
    }

    uint32_t geomId = reinterpret_cast<uint32_t>(updateInfo->movableStaticGeom);

    scene->UpdateTransform(geomId, updateInfo->transform);
    return RG_SUCCESS;
}

RgResult VulkanDevice::UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *uploadInfo)
{
    if (uploadInfo == nullptr || uploadInfo->vertexCount == 0 || uploadInfo->indexCount == 0)
    {
        return RG_WRONG_ARGUMENT;
    }

    rasterizer->Upload(*uploadInfo);
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

    if (enableValidationLayer)
    {
        const char *layerNames[2] =
        {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_LUNARG_monitor"
        };
        instanceInfo.ppEnabledLayerNames = layerNames;
        instanceInfo.enabledLayerCount = 2;
    }

    VkResult r = vkCreateInstance(&instanceInfo, nullptr, &instance);
    VK_CHECKERROR(r);

    InitInstanceExtensionFunctions(instance);

    if (enableValidationLayer)
    {
        assert(debugPrint != nullptr);

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
    if (enableValidationLayer)
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
