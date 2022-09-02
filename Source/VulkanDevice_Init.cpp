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
#include <cstring>
#include <stdexcept>

#include "HaltonSequence.h"
#include "Matrix.h"
#include "RenderResolutionHelper.h"
#include "RgException.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"
#include "LibraryConfig.h"

using namespace RTGL1;

VulkanDevice::VulkanDevice(const RgInstanceCreateInfo *info) :
    instance(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    currentFrameState(),
    frameId(1),
    waitForOutOfFrameFence(false),
    libconfig(LibraryConfig::Read(info->pConfigPath)),
    debugMessenger(VK_NULL_HANDLE),
    userPrint{ std::make_unique<UserPrint>(info->pfnPrint, info->pUserPrintData) },
    userFileLoad{ std::make_shared<UserFileLoad>(info->pfnOpenFile, info->pfnCloseFile, info->pUserLoadFileData) },
    rayCullBackFacingTriangles(info->rayCullBackFacingTriangles),
    allowGeometryWithSkyFlag(info->allowGeometryWithSkyFlag),
    lensFlareVerticesInScreenSpace(info->lensFlareVerticesInScreenSpace),
    previousFrameTime(-1.0 / 60.0),
    currentFrameTime(0)
{
    ValidateCreateInfo(info);



    // init vulkan instance 
    CreateInstance(*info);


    // create VkSurfaceKHR using user's function
    surface = GetSurfaceFromUser(instance, *info);


    // create selected physical device
    physDevice          = std::make_shared<PhysicalDevice>(instance);
    queues              = std::make_shared<Queues>(physDevice->Get(), surface);

    // create vulkan device and set extension function pointers
    CreateDevice();

    CreateSyncPrimitives();

    // set device
    queues->SetDevice(device);


    memAllocator        = std::make_shared<MemoryAllocator>(instance, device, physDevice);

    cmdManager          = std::make_shared<CommandBufferManager>(device, queues);

    uniform             = std::make_shared<GlobalUniform>(device, memAllocator);

    swapchain           = std::make_shared<Swapchain>(device, surface, physDevice->Get(), cmdManager);

    // for world samplers with modifyable lod biad
    worldSamplerManager     = std::make_shared<SamplerManager>(device, 8, info->textureSamplerForceMinificationFilterLinear);
    genericSamplerManager   = std::make_shared<SamplerManager>(device, 0, info->textureSamplerForceMinificationFilterLinear);

    framebuffers        = std::make_shared<Framebuffers>(
        device,
        memAllocator, 
        cmdManager);

    restirBuffers       = std::make_shared<RestirBuffers>(
        device,
        memAllocator);

    blueNoise           = std::make_shared<BlueNoise>(
        device,
        info->pBlueNoiseFilePath,
        memAllocator,
        cmdManager, 
        userFileLoad);

    textureManager      = std::make_shared<TextureManager>(
        device, 
        memAllocator,
        worldSamplerManager,
        cmdManager,
        userFileLoad,
        *info,
        libconfig);

    cubemapManager      = std::make_shared<CubemapManager>(
        device,
        memAllocator,
        genericSamplerManager,
        cmdManager,
        userFileLoad,
        *info,
        libconfig);

    shaderManager       = std::make_shared<ShaderManager>(
        device,
        info->pShaderFolderPath,
        userFileLoad);

    scene               = std::make_shared<Scene>(
        device,
        physDevice,
        memAllocator,
        cmdManager,
        textureManager,
        uniform,
        shaderManager);
   
    rasterizer          = std::make_shared<Rasterizer>(
        device,
        physDevice->Get(),
        shaderManager,
        textureManager,
        uniform,
        genericSamplerManager,
        memAllocator,
        framebuffers,
        cmdManager,
        *info);

    volumetric = std::make_shared< Volumetric >( 
        device,
        cmdManager.get(),
        memAllocator.get(),
        shaderManager.get(),
        uniform.get(),
        blueNoise.get() );

    decalManager        = std::make_shared<DecalManager>(
        device,
        memAllocator,
        shaderManager,
        uniform,
        framebuffers,
        textureManager);

    portalList          = std::make_shared<PortalList>(
        device,
        memAllocator);

    lightGrid           = std::make_shared<LightGrid>(
        device,
        shaderManager,
        uniform,
        blueNoise,
        scene->GetLightManager());

    rtPipeline = std::make_shared< RayTracingPipeline >( 
        device,
        physDevice,
        memAllocator,
        shaderManager.get(),
        scene.get(),
        uniform.get(),
        textureManager.get(),
        framebuffers.get(),
        restirBuffers.get(),
        blueNoise.get(),
        cubemapManager.get(),
        rasterizer->GetRenderCubemap().get(),
        portalList.get(),
        volumetric.get(),
        *info );

    pathTracer          = std::make_shared<PathTracer>(device, rtPipeline);

    tonemapping         = std::make_shared<Tonemapping>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        memAllocator);

    imageComposition    = std::make_shared<ImageComposition>(
        device,
        memAllocator,
        framebuffers, 
        shaderManager, 
        uniform, 
        tonemapping, 
        volumetric.get() );

    bloom               = std::make_shared<Bloom>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        tonemapping);

    amdFsr2             = std::make_shared<FSR2>(
        device,
        physDevice->Get());

    nvDlss              = std::make_shared<DLSS>(
        instance, 
        device, 
        physDevice->Get(),
        info->pAppGUID,
        libconfig.dlssValidation);

    sharpening          = std::make_shared<Sharpening>(
        device,
        framebuffers,
        shaderManager);

    denoiser            = std::make_shared<Denoiser>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        scene->GetASManager());

    effectWipe          = std::make_shared<EffectWipe>(
        device,
        framebuffers,
        uniform,
        blueNoise,
        shaderManager);


#define CONSTRUCT_SIMPLE_EFFECT(T) std::make_shared<T>(device, framebuffers, uniform, shaderManager)
    effectRadialBlur            = CONSTRUCT_SIMPLE_EFFECT(EffectRadialBlur);
    effectChromaticAberration   = CONSTRUCT_SIMPLE_EFFECT(EffectChromaticAberration);
    effectInverseBW             = CONSTRUCT_SIMPLE_EFFECT(EffectInverseBW);
    effectHueShift              = CONSTRUCT_SIMPLE_EFFECT(EffectHueShift);
    effectDistortedSides        = CONSTRUCT_SIMPLE_EFFECT(EffectDistortedSides);
    effectWaves                 = CONSTRUCT_SIMPLE_EFFECT(EffectWaves);
    effectColorTint             = CONSTRUCT_SIMPLE_EFFECT(EffectColorTint);
    effectCrtDemodulateEncode   = CONSTRUCT_SIMPLE_EFFECT(EffectCrtDemodulateEncode);
    effectCrtDecode             = CONSTRUCT_SIMPLE_EFFECT(EffectCrtDecode);
    effectInterlacing           = CONSTRUCT_SIMPLE_EFFECT(EffectInterlacing);
#undef SIMPLE_EFFECT_CONSTRUCTOR_PARAMS


    shaderManager->Subscribe(denoiser);
    shaderManager->Subscribe(imageComposition);
    shaderManager->Subscribe(rasterizer);
    shaderManager->Subscribe(volumetric);
    shaderManager->Subscribe(decalManager);
    shaderManager->Subscribe(rtPipeline);
    shaderManager->Subscribe(lightGrid);
    shaderManager->Subscribe(tonemapping);
    shaderManager->Subscribe(scene->GetVertexPreprocessing());
    shaderManager->Subscribe(bloom);
    shaderManager->Subscribe(sharpening);
    shaderManager->Subscribe(effectWipe);
    shaderManager->Subscribe(effectRadialBlur);
    shaderManager->Subscribe(effectChromaticAberration);
    shaderManager->Subscribe(effectInverseBW);
    shaderManager->Subscribe(effectHueShift);
    shaderManager->Subscribe(effectDistortedSides);
    shaderManager->Subscribe(effectWaves);
    shaderManager->Subscribe(effectColorTint);
    shaderManager->Subscribe(effectCrtDemodulateEncode);
    shaderManager->Subscribe(effectCrtDecode);
    shaderManager->Subscribe(effectInterlacing);

    framebuffers->Subscribe(rasterizer);
    framebuffers->Subscribe(decalManager);
    framebuffers->Subscribe(amdFsr2);
    framebuffers->Subscribe(restirBuffers);
}

VulkanDevice::~VulkanDevice()
{
    vkDeviceWaitIdle(device);

    physDevice.reset();
    queues.reset();
    swapchain.reset();
    cmdManager.reset();
    framebuffers.reset();
    restirBuffers.reset();
    volumetric.reset();
    tonemapping.reset();
    imageComposition.reset();
    bloom.reset();
    amdFsr2.reset();
    nvDlss.reset();
    sharpening.reset();
    effectWipe.reset();
    effectRadialBlur.reset();
    effectChromaticAberration.reset();
    effectInverseBW.reset();
    effectHueShift.reset();
    effectDistortedSides.reset();
    effectWaves.reset();
    effectColorTint.reset();
    effectCrtDemodulateEncode.reset();
    effectCrtDecode.reset();
    effectInterlacing.reset();
    denoiser.reset();
    uniform.reset();
    scene.reset();
    shaderManager.reset();
    rtPipeline.reset();
    pathTracer.reset();
    rasterizer.reset();
    decalManager.reset();
    portalList.reset();
    lightGrid.reset();
    worldSamplerManager.reset();
    genericSamplerManager.reset();
    blueNoise.reset();
    textureManager.reset();
    cubemapManager.reset();
    memAllocator.reset();

    vkDestroySurfaceKHR(instance, surface, nullptr);
    DestroySyncPrimitives();

    DestroyDevice();
    DestroyInstance();
}

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


    // DLSS: ignore error 'VUID-VkCuLaunchInfoNVX-paramCount-arraylength' - 'paramCount must be greater than 0'
    if (pCallbackData->messageIdNumber == 2044605652)
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
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), msg, pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);

    auto *userPrint = static_cast<UserPrint*>(pUserData);
    userPrint->Print(buf);

    return VK_FALSE;
}

void VulkanDevice::CreateInstance(const RgInstanceCreateInfo &info)
{
    std::vector<const char *> layerNames;

    if (libconfig.vulkanValidation)
    {
        layerNames.push_back("VK_LAYER_KHRONOS_validation");
    }

    if (libconfig.fpsMonitor)
    {
        layerNames.push_back("VK_LAYER_LUNARG_monitor");
    }

    std::vector<VkExtensionProperties> supportedInstanceExtensions;
    uint32_t supportedExtensionsCount;

    if (vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionsCount, nullptr) == VK_SUCCESS)
    {
        supportedInstanceExtensions.resize(supportedExtensionsCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionsCount, supportedInstanceExtensions.data());
    }

    std::vector<const char *> extensions =
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,

    #ifdef RG_USE_SURFACE_WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_WIN32

    #ifdef RG_USE_SURFACE_METAL
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_METAL

    #ifdef RG_USE_SURFACE_WAYLAND
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_WAYLAND

    #ifdef RG_USE_SURFACE_XCB
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_XCB

    #ifdef RG_USE_SURFACE_XLIB
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_XLIB
    };

    if (libconfig.vulkanValidation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    for (const char *n : DLSS::GetDlssVulkanInstanceExtensions())
    {
        const bool isSupported = std::any_of(supportedInstanceExtensions.cbegin(), supportedInstanceExtensions.cend(),
            [&](const VkExtensionProperties& ext)
            {
                return !std::strcmp(ext.extensionName, n);
            }
        );

        if (!isSupported)
        {
            continue;
        }

        extensions.push_back(n);
    }


    VkApplicationInfo appInfo = {};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = info.pAppName;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledLayerNames = layerNames.data();
    instanceInfo.enabledLayerCount = layerNames.size();
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    instanceInfo.enabledExtensionCount = extensions.size();

    VkResult r = vkCreateInstance(&instanceInfo, nullptr, &instance);
    VK_CHECKERROR(r);


    if (libconfig.vulkanValidation)
    {
        InitInstanceExtensionFunctions_DebugUtils(instance);

        if (userPrint)
        {
            // init debug utilsdebugMessenger
            VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
            debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debugMessengerInfo.pfnUserCallback = DebugMessengerCallback;
            debugMessengerInfo.pUserData = static_cast<void *>(userPrint.get());

            r = svkCreateDebugUtilsMessengerEXT(instance, &debugMessengerInfo, nullptr, &debugMessenger);
            VK_CHECKERROR(r);
        }
    }
}

void VulkanDevice::CreateDevice()
{
    VkPhysicalDeviceFeatures features = {};
    features.robustBufferAccess = 1;
    features.fullDrawIndexUint32 = 1;
    features.imageCubeArray = 1;
    features.independentBlend = 1;
    features.geometryShader = 0;
    features.tessellationShader = 0;
    features.sampleRateShading = 0;
    features.dualSrcBlend = 0;
    features.logicOp = 1;
    features.multiDrawIndirect = 1;
    features.drawIndirectFirstInstance = 1;
    features.depthClamp = 1;
    features.depthBiasClamp = 1;
    features.fillModeNonSolid = 0;
    features.depthBounds = 1;
    features.wideLines = 0;
    features.largePoints = 0;
    features.alphaToOne = 0;
    features.multiViewport = 1;
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
    features.sparseBinding = 0;
    features.sparseResidencyBuffer = 0;
    features.sparseResidencyImage2D = 0;
    features.sparseResidencyImage3D = 0;
    features.sparseResidency2Samples = 0;
    features.sparseResidency4Samples = 0;
    features.sparseResidency8Samples = 0;
    features.sparseResidency16Samples = 0;
    features.sparseResidencyAliased = 0;
    features.variableMultisampleRate = 0;
    features.inheritedQueries = 1;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.samplerMirrorClampToEdge = 1;
    vulkan12Features.runtimeDescriptorArray = 1;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = 1;
    vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = 1;
    vulkan12Features.bufferDeviceAddress = 1;
    vulkan12Features.shaderFloat16 = 1;
    vulkan12Features.drawIndirectCount = 1;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {};
    multiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.pNext = &vulkan12Features;
    multiviewFeatures.multiview = 1;

    VkPhysicalDevice16BitStorageFeatures storage16 = {};
    storage16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    storage16.pNext = &multiviewFeatures;
    storage16.storageBuffer16BitAccess = 1;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {};
    sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2Features.pNext = &storage16;
    sync2Features.synchronization2 = 1;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &sync2Features;
    rtPipelineFeatures.rayTracingPipeline = 1;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &rtPipelineFeatures;
    asFeatures.accelerationStructure = 1;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &asFeatures;
    physicalDeviceFeatures2.features = features;

    std::vector<VkExtensionProperties> supportedDeviceExtensions;
    uint32_t supportedExtensionsCount;

    if (vkEnumerateDeviceExtensionProperties(physDevice->Get(), nullptr, &supportedExtensionsCount, nullptr) == VK_SUCCESS)
    {
        supportedDeviceExtensions.resize(supportedExtensionsCount);
        vkEnumerateDeviceExtensionProperties(physDevice->Get(), nullptr, &supportedExtensionsCount, supportedDeviceExtensions.data());
    }

    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
    };

    for (const char *n : DLSS::GetDlssVulkanDeviceExtensions())
    {
        const bool isSupported = std::any_of(supportedDeviceExtensions.cbegin(), supportedDeviceExtensions.cend(),
            [&](const VkExtensionProperties& ext)
            {
                return !std::strcmp(ext.extensionName, n);
            }
        );

        if (!isSupported)
        {
            continue;
        }

        deviceExtensions.push_back(n);
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

    if (libconfig.vulkanValidation)
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

    VkFenceCreateInfo nonSignaledFenceInfo = {};
    nonSignaledFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &inFrameSemaphores[i]);
        VK_CHECKERROR(r);

        r = vkCreateFence(device, &fenceInfo, nullptr, &frameFences[i]);
        VK_CHECKERROR(r);
        r = vkCreateFence(device, &nonSignaledFenceInfo, nullptr, &outOfFrameFences[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, imageAvailableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Image available semaphore");
        SET_DEBUG_NAME(device, renderFinishedSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Render finished semaphore");
        SET_DEBUG_NAME(device, inFrameSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "In-frame semaphore");
        SET_DEBUG_NAME(device, frameFences[i], VK_OBJECT_TYPE_FENCE, "Frame fence");
        SET_DEBUG_NAME(device, outOfFrameFences[i], VK_OBJECT_TYPE_FENCE, "Out of frame fence");
    }
}

VkSurfaceKHR VulkanDevice::GetSurfaceFromUser(VkInstance instance, const RgInstanceCreateInfo &info)
{
    VkSurfaceKHR surface;
    VkResult r;


#ifdef RG_USE_SURFACE_WIN32
    if (info.pWin32SurfaceInfo != nullptr)
    {
        VkWin32SurfaceCreateInfoKHR win32Info = {};
        win32Info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        win32Info.hinstance = info.pWin32SurfaceInfo->hinstance;
        win32Info.hwnd = info.pWin32SurfaceInfo->hwnd;

        r = vkCreateWin32SurfaceKHR(instance, &win32Info, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pWin32SurfaceInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pWin32SurfaceInfo is specified, but the library wasn't built with RG_USE_SURFACE_WIN32 option");
    }
#endif // RG_USE_SURFACE_WIN32


#ifdef RG_USE_SURFACE_METAL
    if (info.pMetalSurfaceCreateInfo != nullptr)
    {
        VkMetalSurfaceCreateInfoEXT metalInfo = {};
        metalInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        metalInfo.pLayer = info.pMetalSurfaceCreateInfo->pLayer;

        r = vkCreateMetalSurfaceEXT(instance, &metalInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pMetalSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pMetalSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_METAL option");
    }
#endif // RG_USE_SURFACE_METAL


#ifdef RG_USE_SURFACE_WAYLAND
    if (info.pWaylandSurfaceCreateInfo != nullptr)
    {
        VkWaylandSurfaceCreateInfoKHR wlInfo = {};
        wlInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        wlInfo.display = info.pWaylandSurfaceCreateInfo->display;
        wlInfo.surface = info.pWaylandSurfaceCreateInfo->surface;

        r = (instance, &wlInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pWaylandSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pWaylandSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_WAYLAND option");
    }
#endif // RG_USE_SURFACE_WAYLAND


#ifdef RG_USE_SURFACE_XCB
    if (info.pXcbSurfaceCreateInfo != nullptr)
    {
        VkXcbSurfaceCreateInfoKHR xcbInfo = {};
        xcbInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        xcbInfo.connection = info.pXcbSurfaceCreateInfo->connection;
        xcbInfo.window = info.pXcbSurfaceCreateInfo->window;

        r = vkCreateXcbSurfaceKHR(instance, &xcbInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pXcbSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pXcbSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_XCB option");
    }
#endif // RG_USE_SURFACE_XCB


#ifdef RG_USE_SURFACE_XLIB
    if (info.pXlibSurfaceCreateInfo != nullptr)
    {
        VkXlibSurfaceCreateInfoKHR xlibInfo = {};
        xlibInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        xlibInfo.dpy = info.pXlibSurfaceCreateInfo->dpy;
        xlibInfo.window = info.pXlibSurfaceCreateInfo->window;

        r = vkCreateXlibSurfaceKHR(instance, &xlibInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pXlibSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pXlibSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_XLIB option");
    }
#endif // RG_USE_SURFACE_XLIB


    throw RgException(RG_WRONG_ARGUMENT, "Surface info wasn't specified");
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
        vkDestroySemaphore(device, inFrameSemaphores[i], nullptr);

        vkDestroyFence(device, frameFences[i], nullptr);
        vkDestroyFence(device, outOfFrameFences[i], nullptr);
    }
}

void RTGL1::VulkanDevice::ValidateCreateInfo(const RgInstanceCreateInfo *pInfo)
{
    using namespace std::string_literals;

    if (pInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    {
        int count =
            !!pInfo->pWin32SurfaceInfo +
            !!pInfo->pMetalSurfaceCreateInfo +
            !!pInfo->pWaylandSurfaceCreateInfo +
            !!pInfo->pXcbSurfaceCreateInfo +
            !!pInfo->pXlibSurfaceCreateInfo;

        if (count != 1)
        {
            throw RgException(RG_WRONG_ARGUMENT, "Exactly one of the surface infos must be not null");
        }
    }

    if (pInfo->rasterizedSkyCubemapSize == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "rasterizedSkyCubemapSize must be non-zero");
    }

    if (pInfo->primaryRaysMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "primaryRaysMaxAlbedoLayers must be <="s + std::to_string(MATERIALS_MAX_LAYER_COUNT));
    }

    if (pInfo->indirectIlluminationMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "indirectIlluminationMaxAlbedoLayers must be <="s + std::to_string(MATERIALS_MAX_LAYER_COUNT));
    }
}
