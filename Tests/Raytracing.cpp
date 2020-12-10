#include <assert.h>
#include <cstdio>
#include <vector>
#include <array>
#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "VkUtils.h"
#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "Libs/tinyobjloader/tiny_obj_loader.h"

#define SVK_ENABLE_VALIDATION_LAYER 1
#define MAX_FRAMES_IN_FLIGHT 2

#define MAX_STATIC_VERTICES (1 << 21)
#define SCRATCH_BUFFER_SIZE (1 << 24)
#define MAX_INSTANCE_COUNT 2048 


#define BINDING_VERTEX_BUFFER_STATIC 0
#define BINDING_UNIFORM_BUFFER 0
#define BINDING_RESULT_IMAGE 0
#define BINDING_RAY_AS 0


enum class ShaderIndex
{
    RayGen,
    Miss,
    ShadowMiss,
    ClosestHit,
    SHADER_INDEX_COUNT
};

const char *ShaderNames[] = {
    "../../shaders/raygen.rgen.spv",
    "../../shaders/miss.rmiss.spv",
    "../../shaders/shadow.rmiss.spv",
    "../../shaders/closesthit.rchit.spv"
};

VkShaderStageFlagBits ShaderStages[] = {
    VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    VK_SHADER_STAGE_MISS_BIT_KHR,
    VK_SHADER_STAGE_MISS_BIT_KHR,
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
};


struct Window
{
    GLFWwindow *glfwHandle;
    uint32_t width, height;
    const char **extensions;
    uint32_t extensionCount;
};


struct Vulkan
{
    VkInstance instance;
    VkDevice device;
    VkDebugUtilsMessengerEXT debugMessenger;
    
    VkPhysicalDevice physicalDevice;
    std::vector<VkPhysicalDevice> physicalDevices;
    uint32_t selectedPhysDevice;
    VkPhysicalDeviceMemoryProperties physicalDeviceProperties;
    VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties;

    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    struct
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    } queueFamilyIndices;
    struct
    {
        VkQueue graphics;
        VkQueue compute;
        VkQueue transfer;
    } queues;

    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D surfaceExtent;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
    uint32_t currentSwapchainIndex;

    // [0..MAX_FRAMES_IN_FLIGHT-1]
    uint32_t currentFrameIndex;

    struct
    {
        VkCommandPool   graphics;
        VkCommandPool   compute;
        VkCommandPool   transfer;
    } cmdPools;
    struct
    {
        FrameCmdBuffers graphics[MAX_FRAMES_IN_FLIGHT];
        FrameCmdBuffers compute[MAX_FRAMES_IN_FLIGHT];
        FrameCmdBuffers transfer[MAX_FRAMES_IN_FLIGHT];
    } frameCmds;

    VkFence                 frameFences[MAX_FRAMES_IN_FLIGHT];
    FrameSemaphores         frameSemaphores[MAX_FRAMES_IN_FLIGHT];

    VkFence                 stagingStaticGeomFence;
    
    struct
    {
        VkImage             image;
        VkImageView         view;
        VkDeviceMemory      memory;
        VkFormat            format;
    } outputImage;

    VkPipelineLayout        rtPipelineLayout;
    VkPipeline              rtPipeline;

    VkDescriptorPool        rtDescPool;
    VkDescriptorSetLayout   rtDescSetLayout;
    VkDescriptorSet         rtDescSets[MAX_FRAMES_IN_FLIGHT];

    std::vector<VkShaderModule> rtShaders;

    VkDescriptorPool        storageImageDescPool;
    VkDescriptorSetLayout   storageImageSetLayout;
    VkDescriptorSet         storageImageSets[MAX_FRAMES_IN_FLIGHT];

    Buffer shaderBindingTable;
    uint32_t shaderGroupCount;
};

#define ALIGN_SIZE_4(x, n)  ((x * n + 3) & (~3))

// TODO: autogen to header for c++ and shaders through python
// must have the copy in shaders, data must be aligned to 4
struct StaticVertexBufferData
{
    float positions[ALIGN_SIZE_4(MAX_STATIC_VERTICES, 3)];
    float normals[ALIGN_SIZE_4(MAX_STATIC_VERTICES, 3)];
};

// this data will be set to StaticVertexBufferData
std::vector<float> _positions;
std::vector<float> _normals;
//std::vector<uint32_t> _indices;


struct UniformData
{
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::vec4 lightPos;
};

struct RtglData
{
    Buffer                      staticVertsStaging;
    Buffer                      staticVerts;

    VkAccelerationStructureKHR  staticBlas;
    VkDeviceMemory              staticBlasMemory;

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    Buffer                      instanceBuffer[MAX_FRAMES_IN_FLIGHT];

    VkAccelerationStructureKHR  tlas[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory              tlasMemory[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool            vertsDescPool;
    VkDescriptorSetLayout       staticVertsDescSetLayout;
    VkDescriptorSet             staticVertsDescSet;
    
    // common scratch buffer
    Buffer                      scratchBuffer;
    VkDeviceAddress             scratchBufferCurrentOffset;


    // uniform
    Buffer                      uniformBuffers[MAX_FRAMES_IN_FLIGHT];
    UniformData                 uniformData;

    VkDescriptorPool            uniformDescPool;
    VkDescriptorSetLayout       uniformDescSetLayout;
    VkDescriptorSet             uniformDescSets[MAX_FRAMES_IN_FLIGHT];
};

Vulkan mainVk;
RtglData rtglData;


void CreateInstance(const Window &window)
{
    std::vector<const char*> extensions;

    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#if SVK_ENABLE_VALIDATION_LAYER
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif // SVK_ENABLE_VALIDATION_LAYER

    for (uint32_t i = 0; i < window.extensionCount; i++)
    {
        extensions.push_back(window.extensions[i]);
    }

    VkApplicationInfo appInfo = {};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = "Raytracing test";

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    instanceInfo.enabledExtensionCount = extensions.size();

#if SVK_ENABLE_VALIDATION_LAYER
    const char *layerNames[2] = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_monitor"
    };
    instanceInfo.ppEnabledLayerNames = layerNames;
    instanceInfo.enabledLayerCount = 2;
#endif

    VkResult r = vkCreateInstance(&instanceInfo, nullptr, &mainVk.instance);
    VK_CHECKERROR(r);

    InitInstanceExtensionFunctions(mainVk.instance);

    // init debug utils
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
    debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugMessengerInfo.pfnUserCallback = DebugMessengerCallback;

    r = vksCreateDebugUtilsMessengerEXT(mainVk.instance, &debugMessengerInfo, nullptr, &mainVk.debugMessenger);
    VK_CHECKERROR(r);
}


void CreateDevice()
{
    VkResult r;

    uint32_t physCount = 0;
    r = vkEnumeratePhysicalDevices(mainVk.instance, &physCount, nullptr);
    assert(physCount > 0);

    mainVk.physicalDevices.resize(physCount);
    r = vkEnumeratePhysicalDevices(mainVk.instance, &physCount, mainVk.physicalDevices.data());
    VK_CHECKERROR(r);

    mainVk.selectedPhysDevice = 0;
    mainVk.physicalDevice = mainVk.physicalDevices[mainVk.selectedPhysDevice];
    VkPhysicalDevice physDevice = mainVk.physicalDevice;

    vkGetPhysicalDeviceMemoryProperties(physDevice, &mainVk.physicalDeviceProperties);

    mainVk.rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProp2 = {};
    deviceProp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProp2.pNext = &mainVk.rayTracingProperties;
    vkGetPhysicalDeviceProperties2(physDevice, &deviceProp2);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);

    mainVk.queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, mainVk.queueFamilyProperties.data());

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo queueInfo = {};
    const float defaultQueuePriority = 0;
   
    mainVk.queueFamilyIndices.graphics = GetQueueFamilyIndex(mainVk.queueFamilyProperties, VK_QUEUE_GRAPHICS_BIT);
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = mainVk.queueFamilyIndices.graphics;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &defaultQueuePriority;
    queueCreateInfos.push_back(queueInfo);

    mainVk.queueFamilyIndices.compute = GetQueueFamilyIndex(mainVk.queueFamilyProperties, VK_QUEUE_COMPUTE_BIT);
    if (mainVk.queueFamilyIndices.compute != mainVk.queueFamilyIndices.graphics)
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = mainVk.queueFamilyIndices.compute;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    mainVk.queueFamilyIndices.transfer = GetQueueFamilyIndex(mainVk.queueFamilyProperties, VK_QUEUE_TRANSFER_BIT);
    if ((mainVk.queueFamilyIndices.transfer != mainVk.queueFamilyIndices.graphics) 
        && (mainVk.queueFamilyIndices.transfer != mainVk.queueFamilyIndices.compute))
    {
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = mainVk.queueFamilyIndices.transfer;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

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

    VkPhysicalDeviceRayTracingFeaturesKHR rtFeatures = {};
    rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_FEATURES_KHR;
    rtFeatures.pNext = &bufferAddressFeatures;
    rtFeatures.rayTracing = VK_TRUE;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &rtFeatures;
    physicalDeviceFeatures2.features = features;

    std::vector<const char *> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
#if SVK_ENABLE_VALIDATION_LAYER
    deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;
    deviceCreateInfo.enabledExtensionCount = (uint32_t) deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    VkResult result = vkCreateDevice(mainVk.physicalDevice, &deviceCreateInfo, nullptr, &mainVk.device);
    assert(result == VK_SUCCESS);

    InitDeviceExtensionFunctions(mainVk.device);

    vkGetDeviceQueue(mainVk.device, mainVk.queueFamilyIndices.graphics, 0, &mainVk.queues.graphics);
    vkGetDeviceQueue(mainVk.device, mainVk.queueFamilyIndices.compute, 0, &mainVk.queues.compute);
    vkGetDeviceQueue(mainVk.device, mainVk.queueFamilyIndices.transfer, 0, &mainVk.queues.transfer);
}


void CreateSwapchain(bool vsync, uint32_t windowWidth, uint32_t windowHeight)
{
    VkResult r;
    VkSwapchainKHR oldSwapchain = mainVk.swapchain;

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mainVk.physicalDevice, mainVk.surface, &formatCount, NULL);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mainVk.physicalDevice, mainVk.surface, &formatCount, surfaceFormats.data());

    std::vector<VkFormat> acceptFormats;
    acceptFormats.push_back(VK_FORMAT_R8G8B8A8_SRGB);
    acceptFormats.push_back(VK_FORMAT_B8G8R8A8_SRGB);

    for (VkFormat f : acceptFormats)
    {
        for (VkSurfaceFormatKHR sf : surfaceFormats)
        {
            if (sf.format == f)
            {
                mainVk.surfaceFormat = sf;
            }
        }

        if (mainVk.surfaceFormat.format != VK_FORMAT_UNDEFINED)
            break;
    }

    if (vsync)
    {
        mainVk.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
    else
    {
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(mainVk.physicalDevice, mainVk.surface, &presentModeCount, NULL);

        std::vector<VkPresentModeKHR> presentModes;
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(mainVk.physicalDevice, mainVk.surface, &presentModeCount, presentModes.data());

        bool foundImmediate = false;

        for (auto p : presentModes)
        {
            if (p == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                foundImmediate = true;
                break;
            }
        }

        mainVk.presentMode = foundImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
    }

    VkSurfaceCapabilitiesKHR surfCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mainVk.physicalDevice, mainVk.surface, &surfCapabilities);

    if (surfCapabilities.currentExtent.width == UINT32_MAX &&
        surfCapabilities.currentExtent.height == UINT32_MAX)
    {
        mainVk.surfaceExtent = surfCapabilities.currentExtent;
    }
    else
    {
        mainVk.surfaceExtent.width = std::min(surfCapabilities.maxImageExtent.width, windowWidth);
        mainVk.surfaceExtent.height = std::min(surfCapabilities.maxImageExtent.height, windowHeight);

        mainVk.surfaceExtent.width = std::max(surfCapabilities.minImageExtent.width, mainVk.surfaceExtent.width);
        mainVk.surfaceExtent.height = std::max(surfCapabilities.minImageExtent.height, mainVk.surfaceExtent.height);
    }

    VkBool32 supported;
    r = vkGetPhysicalDeviceSurfaceSupportKHR(mainVk.physicalDevice, mainVk.queueFamilyIndices.graphics, mainVk.surface, &supported);
    VK_CHECKERROR(r);
    assert(supported);

    uint32_t imageCount = 2;
    if (surfCapabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, surfCapabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = mainVk.surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = mainVk.surfaceFormat.format;
    swapchainInfo.imageColorSpace = mainVk.surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = mainVk.surfaceExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = surfCapabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = mainVk.presentMode;
    swapchainInfo.clipped = VK_FALSE;
    swapchainInfo.oldSwapchain = oldSwapchain;

    r = vkCreateSwapchainKHR(mainVk.device, &swapchainInfo, NULL, &mainVk.swapchain);
    VK_CHECKERROR(r);

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (VkImageView iv : mainVk.swapchainViews)
        {
            vkDestroyImageView(mainVk.device, iv, nullptr);
        }

        vkDestroySwapchainKHR(mainVk.device, oldSwapchain, nullptr);
    }

    vkGetSwapchainImagesKHR(mainVk.device, mainVk.swapchain, &imageCount, nullptr);
    mainVk.swapchainImages.resize(imageCount);
    mainVk.swapchainViews.resize(imageCount);
    vkGetSwapchainImagesKHR(mainVk.device, mainVk.swapchain, &imageCount, mainVk.swapchainImages.data());

    for (uint32_t i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mainVk.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mainVk.surfaceFormat.format;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        r = vkCreateImageView(mainVk.device, &viewInfo, nullptr, &mainVk.swapchainViews[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(mainVk.device, (uint64_t) mainVk.swapchainImages[i], VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Swapchain image");
        SET_DEBUG_NAME(mainVk.device, (uint64_t) mainVk.swapchainViews[i], VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Swapchain image view");
    }

    for (uint32_t i = 0; i < imageCount; i++)
    {
        FrameCmdBuffers &frameCmds = mainVk.frameCmds.graphics[mainVk.currentFrameIndex];

        VkCommandBuffer cmd = frameCmds.BeginCmd();

        BarrierImage(cmd, mainVk.swapchainImages[i],
                     0, 0,
                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        frameCmds.Submit(cmd);
        frameCmds.WaitIdle();
    }
}


void CreateCmdPools()
{
    VkResult r;

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    cmdPoolInfo.queueFamilyIndex = mainVk.queueFamilyIndices.graphics;
    r = vkCreateCommandPool(mainVk.device, &cmdPoolInfo, nullptr, &mainVk.cmdPools.graphics);
    VK_CHECKERROR(r);

    cmdPoolInfo.queueFamilyIndex = mainVk.queueFamilyIndices.compute;
    r = vkCreateCommandPool(mainVk.device, &cmdPoolInfo, nullptr, &mainVk.cmdPools.compute);
    VK_CHECKERROR(r);

    cmdPoolInfo.queueFamilyIndex = mainVk.queueFamilyIndices.transfer;
    r = vkCreateCommandPool(mainVk.device, &cmdPoolInfo, nullptr, &mainVk.cmdPools.transfer);
    VK_CHECKERROR(r);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        mainVk.frameCmds.graphics[i] = FrameCmdBuffers(mainVk.device, mainVk.cmdPools.graphics, mainVk.queues.graphics);
        mainVk.frameCmds.compute[i] = FrameCmdBuffers(mainVk.device, mainVk.cmdPools.compute, mainVk.queues.compute);
        mainVk.frameCmds.transfer[i] = FrameCmdBuffers(mainVk.device, mainVk.cmdPools.transfer, mainVk.queues.transfer);
    }
}


void CreateSyncPrimitives()
{
    VkResult r;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkCreateSemaphore(mainVk.device, &semaphoreInfo, nullptr, &mainVk.frameSemaphores[i].imageAvailable); VK_CHECKERROR(r);
        r = vkCreateSemaphore(mainVk.device, &semaphoreInfo, nullptr, &mainVk.frameSemaphores[i].renderFinished); VK_CHECKERROR(r);
        r = vkCreateSemaphore(mainVk.device, &semaphoreInfo, nullptr, &mainVk.frameSemaphores[i].traceFinished); VK_CHECKERROR(r);
        r = vkCreateSemaphore(mainVk.device, &semaphoreInfo, nullptr, &mainVk.frameSemaphores[i].transferFinished); VK_CHECKERROR(r);
    
        r = vkCreateFence(mainVk.device, &fenceInfo, nullptr, &mainVk.frameFences[i]); VK_CHECKERROR(r);
    }

    r = vkCreateFence(mainVk.device, &fenceInfo, nullptr, &mainVk.stagingStaticGeomFence); VK_CHECKERROR(r);
}


void CreateVertexBuffers()
{
    VkResult r;

    CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                 sizeof(StaticVertexBufferData),
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 rtglData.staticVertsStaging);

    CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                 sizeof(StaticVertexBufferData),
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 rtglData.staticVerts);

    CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                 SCRATCH_BUFFER_SIZE,
                 VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 rtglData.scratchBuffer);

    VkDescriptorSetLayoutBinding staticVertsBinding = {};
    staticVertsBinding.binding = BINDING_VERTEX_BUFFER_STATIC;
    staticVertsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    staticVertsBinding.descriptorCount = 1;
    staticVertsBinding.stageFlags = VK_SHADER_STAGE_ALL;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(staticVertsBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    r = vkCreateDescriptorSetLayout(mainVk.device, &layoutInfo, nullptr, &rtglData.staticVertsDescSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = bindings.size();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    r = vkCreateDescriptorPool(mainVk.device, &poolInfo, nullptr, &rtglData.vertsDescPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = rtglData.vertsDescPool;
    descSetInfo.descriptorSetCount = 1;
    descSetInfo.pSetLayouts = &rtglData.staticVertsDescSetLayout;

    r = vkAllocateDescriptorSets(mainVk.device, &descSetInfo, &rtglData.staticVertsDescSet);
    VK_CHECKERROR(r);

    VkDescriptorBufferInfo bufferInfo = {};
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.dstSet = rtglData.staticVertsDescSet;
    write.dstArrayElement = 0;
    write.pBufferInfo = &bufferInfo;

    write.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
    bufferInfo.buffer = rtglData.staticVerts.buffer;
    bufferInfo.range = sizeof(StaticVertexBufferData);

    vkUpdateDescriptorSets(mainVk.device, 1, &write, 0, nullptr);
}


void DestroyVertexBuffers()
{
    DestroyBuffer(mainVk.device, rtglData.staticVertsStaging.buffer, rtglData.staticVertsStaging.memory);
    DestroyBuffer(mainVk.device, rtglData.staticVerts.buffer, rtglData.staticVerts.memory);

    vkDestroyDescriptorSetLayout(mainVk.device, rtglData.staticVertsDescSetLayout, nullptr);
    vkDestroyDescriptorPool(mainVk.device, rtglData.vertsDescPool, nullptr);

    rtglData.staticVertsStaging = {};
    rtglData.staticVerts = {};
    rtglData.staticVertsDescSetLayout = VK_NULL_HANDLE;
    rtglData.vertsDescPool = VK_NULL_HANDLE;
}


void CreateInstanceBuffers()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                     MAX_INSTANCE_COUNT * sizeof(VkTransformMatrixKHR),
                     VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     rtglData.instanceBuffer[i]);
    }
}


void UploadVertexData()
{
    VkResult r;
    assert(_positions.size() <= MAX_STATIC_VERTICES);

    StaticVertexBufferData *mapped = (StaticVertexBufferData *) rtglData.staticVertsStaging.Map(mainVk.device);
    memcpy(mapped->positions, _positions.data(), _positions.size() * sizeof(float));
    memcpy(mapped->normals, _normals.data(), _normals.size() * sizeof(float));

    rtglData.staticVertsStaging.Unmap(mainVk.device);

    VkFence fence = mainVk.stagingStaticGeomFence;

    // copy from staging
    r = vkWaitForFences(mainVk.device, 1, &fence, VK_TRUE, UINT64_MAX);
    VK_CHECKERROR(r);
    vkResetFences(mainVk.device, 1, &fence);

    VkCommandBuffer cmd = mainVk.frameCmds.graphics[mainVk.currentFrameIndex].BeginCmd();

    VkBufferCopy copyRegion = {};
    copyRegion.size = sizeof(StaticVertexBufferData);
    vkCmdCopyBuffer(cmd, rtglData.staticVertsStaging.buffer, rtglData.staticVerts.buffer, 1, &copyRegion);

    VkBufferMemoryBarrier bufferMemBarrier = {};
    bufferMemBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferMemBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    bufferMemBarrier.buffer = rtglData.staticVerts.buffer;
    bufferMemBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 1, &bufferMemBarrier,
                         0, nullptr);

    mainVk.frameCmds.graphics[mainVk.currentFrameIndex].Submit(cmd, fence);
}


void BuildBottomAS(VkCommandBuffer cmd, VkAccelerationStructureKHR as, bool fastTrace);

void CreateBottomAS(bool fastTrace)
{
    VkResult r;
    uint32_t vertCount = _positions.size() / 3;
    //uint32_t primitiveCount = _indices.size() / 3;
    uint32_t primitiveCount = vertCount / 3;

    VkBuildAccelerationStructureFlagsKHR buildFlags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    VkAccelerationStructureCreateGeometryTypeInfoKHR geomTypeInfo = {};
    geomTypeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    geomTypeInfo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geomTypeInfo.maxPrimitiveCount = primitiveCount;
    geomTypeInfo.indexType = VK_INDEX_TYPE_NONE_KHR;
    geomTypeInfo.maxVertexCount = vertCount;
    geomTypeInfo.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geomTypeInfo.allowsTransforms = VK_FALSE;

    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    asInfo.flags = buildFlags;
    asInfo.maxGeometryCount = 1;
    asInfo.pGeometryInfos = &geomTypeInfo;
    r = vksCreateAccelerationStructureKHR(mainVk.device, &asInfo, nullptr, &rtglData.staticBlas);
    VK_CHECKERROR(r);

    AllocASMemory(mainVk.device, mainVk.physicalDeviceProperties, rtglData.staticBlas, &rtglData.staticBlasMemory);
    BindASMemory(mainVk.device, rtglData.staticBlas, rtglData.staticBlasMemory);

    // TODO: move this out
    FrameCmdBuffers &frameCmds = mainVk.frameCmds.graphics[mainVk.currentFrameIndex];
    VkCommandBuffer cmd = frameCmds.BeginCmd();

    rtglData.scratchBufferCurrentOffset = 0;
    BuildBottomAS(cmd, rtglData.staticBlas, true);

    // scratch data sync
    VkMemoryBarrier memBarrier = {};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    memBarrier.srcAccessMask = 
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | 
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    memBarrier.dstAccessMask = 
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd, 
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 
                         1, &memBarrier,
                         0, 0, 
                         0, 0);

    rtglData.scratchBufferCurrentOffset = 0;

    // here can be other BuildBottomAS(..)

    frameCmds.Submit(cmd);
    frameCmds.WaitIdle();
}


void BuildBottomAS(VkCommandBuffer cmd, VkAccelerationStructureKHR as, bool fastTrace)
{
    uint32_t vertCount = _positions.size() / 3;
    uint32_t primitiveCount = vertCount / 3;

    VkBuildAccelerationStructureFlagsKHR buildFlags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    //
    VkDeviceOrHostAddressKHR scratchData = {};
    scratchData.deviceAddress = rtglData.scratchBuffer.address + rtglData.scratchBufferCurrentOffset;

    VkAccelerationStructureMemoryRequirementsInfoKHR scratchMemReqInfo = {};
    scratchMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    scratchMemReqInfo.accelerationStructure = as;
    scratchMemReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
    
    VkMemoryRequirements2 memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vksGetAccelerationStructureMemoryRequirementsKHR(mainVk.device, &scratchMemReqInfo, &memReq2);

    VkDeviceSize scratchSize = memReq2.memoryRequirements.size;
    rtglData.scratchBufferCurrentOffset += scratchSize;
    //

    VkAccelerationStructureGeometryTrianglesDataKHR trData = {};
    trData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trData.vertexData.deviceAddress = rtglData.staticVerts.address;
    trData.vertexStride = 3 * sizeof(float);
    trData.indexType = VK_INDEX_TYPE_NONE_KHR;

    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.geometry.triangles = trData;

    VkAccelerationStructureGeometryKHR *ppGeom[] = { &geom };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = buildFlags;
    buildInfo.update = VK_FALSE;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.geometryArrayOfPointers = VK_TRUE;
    buildInfo.geometryCount = 1;
    buildInfo.ppGeometries = ppGeom;
    buildInfo.scratchData = scratchData;

    VkAccelerationStructureBuildOffsetInfoKHR offset = {};
    offset.primitiveCount = primitiveCount;

    VkAccelerationStructureBuildOffsetInfoKHR *ppOffsets[] = { &offset };

    vksCmdBuildAccelerationStructureKHR(cmd, 1, &buildInfo, ppOffsets);
}


void BuildTopLevelAS(VkCommandBuffer cmd, VkAccelerationStructureKHR as, bool fastTrace);

void CreateTopLevelAS(bool fastTrace, uint32_t frameIndex)
{
    VkResult r;

    VkAccelerationStructureKHR *pTlas = &rtglData.tlas[frameIndex];
    VkDeviceMemory *pTlasMemory = &rtglData.tlasMemory[frameIndex];

    VkBuildAccelerationStructureFlagsKHR buildFlags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    auto &instances = rtglData.instances;

    // for each isntance
    {
        VkTransformMatrixKHR transformMatrix =
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };

        VkAccelerationStructureInstanceKHR instance = {};
        instance.transform = transformMatrix;
        instance.instanceCustomIndex = 0;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = GetASDeviceAddress(mainVk.device, rtglData.staticBlas);
    
        instances.push_back(instance);
    }

    assert(instances.size() <= MAX_INSTANCE_COUNT);

    Buffer &instanceBuffer = rtglData.instanceBuffer[frameIndex];

    void *mapped = instanceBuffer.Map(mainVk.device);
    memcpy(mapped, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
 
    instanceBuffer.Unmap(mainVk.device);


    VkAccelerationStructureCreateGeometryTypeInfoKHR geomTypeInfo = {};
    geomTypeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    geomTypeInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geomTypeInfo.maxPrimitiveCount = instances.size();

    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    asInfo.flags = buildFlags;
    asInfo.maxGeometryCount = 1;
    asInfo.pGeometryInfos = &geomTypeInfo;
    r = vksCreateAccelerationStructureKHR(mainVk.device, &asInfo, nullptr, pTlas);
    VK_CHECKERROR(r);
    
    AllocASMemory(mainVk.device, mainVk.physicalDeviceProperties, *pTlas, pTlasMemory);
    BindASMemory(mainVk.device, *pTlas, *pTlasMemory);


    //
    FrameCmdBuffers &frameCmds = mainVk.frameCmds.graphics[frameIndex];
    VkCommandBuffer cmd = frameCmds.BeginCmd();

    rtglData.scratchBufferCurrentOffset = 0;
    BuildTopLevelAS(cmd, *pTlas, true);

    frameCmds.Submit(cmd);
    frameCmds.WaitIdle();
}


void BuildTopLevelAS(VkCommandBuffer cmd, VkAccelerationStructureKHR as, bool fastTrace)
{
    uint32_t primitiveCount = rtglData.instances.size();

    VkBuildAccelerationStructureFlagsKHR buildFlags = fastTrace ?
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR :
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    //
    rtglData.scratchBufferCurrentOffset = 0;

    VkDeviceOrHostAddressKHR scratchData = {};
    scratchData.deviceAddress = rtglData.scratchBuffer.address + rtglData.scratchBufferCurrentOffset;
    //
 
    VkAccelerationStructureGeometryInstancesDataKHR instData = {};
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = rtglData.instanceBuffer[mainVk.currentFrameIndex].address;

    VkAccelerationStructureGeometryKHR instGeom = {};
    instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    instGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    instGeom.geometry.instances = instData;

    VkAccelerationStructureGeometryKHR *ppGeom[] = { &instGeom };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = buildFlags;
    buildInfo.update = VK_FALSE;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.geometryArrayOfPointers = VK_TRUE;
    buildInfo.geometryCount = 1;
    buildInfo.ppGeometries = ppGeom;
    buildInfo.scratchData = scratchData;

    VkAccelerationStructureBuildOffsetInfoKHR offset = {};
    offset.primitiveCount = primitiveCount;

    VkAccelerationStructureBuildOffsetInfoKHR *ppOffsets[] = { &offset };

    vksCmdBuildAccelerationStructureKHR(cmd, 1, &buildInfo, ppOffsets);
}


void CreateStorageImage(uint32_t width, uint32_t height)
{
    VkResult r;

    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    r = vkCreateImage(mainVk.device, &imageInfo, nullptr, &mainVk.outputImage.image);
    VK_CHECKERROR(r);
    SET_DEBUG_NAME(mainVk.device, (uint64_t) mainVk.outputImage.image, VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Output image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mainVk.device, mainVk.outputImage.image, &memReqs);

    mainVk.outputImage.memory = AllocDeviceMemory(mainVk.device, mainVk.physicalDeviceProperties, memReqs);
    
    r = vkBindImageMemory(mainVk.device, mainVk.outputImage.image, mainVk.outputImage.memory, 0);
    VK_CHECKERROR(r);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = mainVk.outputImage.image;
    r = vkCreateImageView(mainVk.device, &viewInfo, nullptr, &mainVk.outputImage.view);
    VK_CHECKERROR(r);
    SET_DEBUG_NAME(mainVk.device, (uint64_t) mainVk.outputImage.view, VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Output image view");

    VkCommandBuffer cmd = mainVk.frameCmds.graphics[mainVk.currentFrameIndex].BeginCmd();

    BarrierImage(cmd, mainVk.outputImage.image,
                 0, VK_ACCESS_SHADER_WRITE_BIT,
                 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    mainVk.frameCmds.graphics[mainVk.currentFrameIndex].Submit(cmd);


    //
    VkDescriptorSetLayoutBinding storageImageBinding = {};
    storageImageBinding.binding = BINDING_RESULT_IMAGE;
    storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    storageImageBinding.descriptorCount = 1;
    storageImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &storageImageBinding;

    r = vkCreateDescriptorSetLayout(mainVk.device, &layoutInfo, nullptr, &mainVk.storageImageSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(mainVk.device, &poolInfo, nullptr, &mainVk.storageImageDescPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mainVk.storageImageDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mainVk.storageImageSetLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(mainVk.device, &allocInfo, &mainVk.storageImageSets[i]);
        VK_CHECKERROR(r);
    }


    //
    VkDescriptorImageInfo imageInfos[MAX_FRAMES_IN_FLIGHT] = {};
    VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT] = {};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        imageInfos[i].imageView = mainVk.outputImage.view;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = mainVk.storageImageSets[i];
        writes[i].dstBinding = BINDING_RESULT_IMAGE;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }

    vkUpdateDescriptorSets(mainVk.device, MAX_FRAMES_IN_FLIGHT, writes, 0, nullptr);
}


void CreateUniformBuffer()
{
    VkResult r;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                     sizeof(UniformData),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     rtglData.uniformBuffers[i]);
    }

    
    //
    VkDescriptorSetLayoutBinding uniformBinding = {};
    uniformBinding.binding = BINDING_UNIFORM_BUFFER;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uniformBinding;

    r = vkCreateDescriptorSetLayout(mainVk.device, &layoutInfo, nullptr, &rtglData.uniformDescSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(mainVk.device, &poolInfo, nullptr, &rtglData.uniformDescPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = rtglData.uniformDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &rtglData.uniformDescSetLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(mainVk.device, &allocInfo, &rtglData.uniformDescSets[i]);
        VK_CHECKERROR(r);
    }


    //
    VkDescriptorBufferInfo bufferInfos[MAX_FRAMES_IN_FLIGHT] = {};
    VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT] = {};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        bufferInfos[i].buffer = rtglData.uniformBuffers[i].buffer;
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = sizeof(UniformData);

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = rtglData.uniformDescSets[i];
        writes[i].dstBinding = BINDING_UNIFORM_BUFFER;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(mainVk.device, MAX_FRAMES_IN_FLIGHT, writes, 0, nullptr);
}

glm::vec3 camPos = glm::vec3(0, 50, 0);
glm::vec3 camDir = glm::vec3(0, 0, 1);
glm::vec3 camUp = glm::vec3(0, 1, 0);
glm::vec3 lightDir = glm::vec3(1, 1, 1);

void UpdateUniformBuffer()
{
    glm::mat4 persp = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 10000.0f);
    glm::mat4 view = glm::lookAt(camPos, camPos + camDir, camUp);

    rtglData.uniformData.viewInverse = glm::inverse(view);
    rtglData.uniformData.projInverse = glm::inverse(persp);
    rtglData.uniformData.lightPos = glm::vec4(lightDir.x, lightDir.y, lightDir.z, 0);

    Buffer &ub = rtglData.uniformBuffers[mainVk.currentFrameIndex];

    void *mapped = ub.Map(mainVk.device);
    memcpy(mapped, &rtglData.uniformData, sizeof(UniformData));
 
    ub.Unmap(mainVk.device);
}


void CreateRayTracingDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding asBinding = {};
    asBinding.binding = BINDING_RAY_AS;
    asBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asBinding.descriptorCount = 1;
    asBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &asBinding;

    r = vkCreateDescriptorSetLayout(mainVk.device, &layoutInfo, nullptr, &mainVk.rtDescSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    r = vkCreateDescriptorPool(mainVk.device, &poolInfo, nullptr, &mainVk.rtDescPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mainVk.rtDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mainVk.rtDescSetLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(mainVk.device, &allocInfo, &mainVk.rtDescSets[i]);
        VK_CHECKERROR(r);
    }
}


VkShaderModule LoadShader(const char *name)
{
    std::ifstream shaderFile(name, std::ios::binary);
    std::vector<uint8_t> shaderSource(std::istreambuf_iterator<char>(shaderFile), {});
    assert(shaderSource.size() > 0);

    VkShaderModule shaderModule;

    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = shaderSource.size();
    moduleInfo.pCode = (uint32_t *) shaderSource.data();

    VkResult r = vkCreateShaderModule(mainVk.device, &moduleInfo, nullptr, &shaderModule);
    VK_CHECKERROR(r);

    return shaderModule;
}


void LoadShaders()
{
    auto &shaders = mainVk.rtShaders;

    for (uint32_t i = 0; i < (uint32_t) ShaderIndex::SHADER_INDEX_COUNT; i++)
    {
        shaders.push_back(LoadShader(ShaderNames[i]));
    }
}


VkPipelineShaderStageCreateInfo GetShaderStage(ShaderIndex index, VkShaderStageFlagBits stage)
{
    auto &shaders = mainVk.rtShaders;

    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = shaders[(uint32_t)index];
    shaderStage.pName = "main";
    return shaderStage;
}


void CreateRayTracingPipeline()
{
    VkResult r;
    const uint32_t shaderCount = (uint32_t) ShaderIndex::SHADER_INDEX_COUNT;

    VkPipelineShaderStageCreateInfo stageInfos[shaderCount] = {};
    for (uint32_t i = 0; i < shaderCount; i++)
    {
        stageInfos[i] = GetShaderStage((ShaderIndex)i, ShaderStages[i]);
    }

    VkDescriptorSetLayout setLayouts[] = {
        mainVk.rtDescSetLayout,
        mainVk.storageImageSetLayout, 
        rtglData.uniformDescSetLayout,
        rtglData.staticVertsDescSetLayout
    };

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = 4;
    plLayoutInfo.pSetLayouts = setLayouts;

    r = vkCreatePipelineLayout(mainVk.device, &plLayoutInfo, nullptr, &mainVk.rtPipelineLayout);
    VK_CHECKERROR(r);

    VkRayTracingShaderGroupCreateInfoKHR groups[4] = {};

    VkRayTracingShaderGroupCreateInfoKHR &raygenGroup = groups[0];
    raygenGroup.sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader       = (uint32_t)ShaderIndex::RayGen;
    raygenGroup.closestHitShader    = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader        = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader  = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR &missGroup = groups[1];
    missGroup.sType                 = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type                  = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader         = (uint32_t) ShaderIndex::Miss;
    missGroup.closestHitShader      = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader          = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader    = VK_SHADER_UNUSED_KHR;
    
    VkRayTracingShaderGroupCreateInfoKHR &missShGroup = groups[2];
    missShGroup.sType               = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missShGroup.type                = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShGroup.generalShader       = (uint32_t) ShaderIndex::ShadowMiss;
    missShGroup.closestHitShader    = VK_SHADER_UNUSED_KHR;
    missShGroup.anyHitShader        = VK_SHADER_UNUSED_KHR;
    missShGroup.intersectionShader  = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR &clHitGroup = groups[3];
    clHitGroup.sType                = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    clHitGroup.type                 = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    clHitGroup.generalShader        = VK_SHADER_UNUSED_KHR;
    clHitGroup.closestHitShader     = (uint32_t) ShaderIndex::ClosestHit;
    clHitGroup.anyHitShader         = VK_SHADER_UNUSED_KHR;
    clHitGroup.intersectionShader   = VK_SHADER_UNUSED_KHR;

    mainVk.shaderGroupCount = 4;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = shaderCount;
    pipelineInfo.pStages = stageInfos;
    pipelineInfo.groupCount = mainVk.shaderGroupCount;
    pipelineInfo.pGroups = groups;
    pipelineInfo.maxRecursionDepth = 2;
    pipelineInfo.layout = mainVk.rtPipelineLayout;
    pipelineInfo.libraries.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    r = vksCreateRayTracingPipelinesKHR(mainVk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mainVk.rtPipeline);
    VK_CHECKERROR(r);
}


void UpdateASDescSetBinding(uint32_t frameIndex)
{
    VkWriteDescriptorSetAccelerationStructureKHR descSetAS = {};
    descSetAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descSetAS.accelerationStructureCount = 1;
    descSetAS.pAccelerationStructures = &rtglData.tlas[frameIndex];

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = &descSetAS;
    write.dstSet = mainVk.rtDescSets[frameIndex];
    write.dstBinding = BINDING_RAY_AS;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write.descriptorCount = 1;

    vkUpdateDescriptorSets(mainVk.device, 1, &write, 0, nullptr);
}


void CreateShaderBindingTable()
{
    VkResult r;
    uint32_t groupCount = mainVk.shaderGroupCount;
    VkDeviceSize sbtAlignment = mainVk.rayTracingProperties.shaderGroupBaseAlignment;
    VkDeviceSize sbtHandleSize = mainVk.rayTracingProperties.shaderGroupHandleSize;
    VkDeviceSize sbtSize = sbtAlignment * groupCount;

    CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                 sbtSize,
                 VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                 mainVk.shaderBindingTable);

    std::vector<uint8_t> shaderHandles(sbtSize);
    r = vksGetRayTracingShaderGroupHandlesKHR(mainVk.device, mainVk.rtPipeline, 0, groupCount,
                                             shaderHandles.size(), shaderHandles.data());
    VK_CHECKERROR(r);

    uint8_t *mapped = (uint8_t*)mainVk.shaderBindingTable.Map(mainVk.device);

    for (uint32_t i = 0; i < groupCount; i++)
    {
        memcpy(mapped, shaderHandles.data() + i * sbtHandleSize, sbtHandleSize);
        mapped += sbtAlignment;
    }

    mainVk.shaderBindingTable.Unmap(mainVk.device);
}


void LoadModel(const char *path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);
    assert(ret);

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            int fv = shapes[s].mesh.num_face_vertices[f];
            assert(fv == 3);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
                tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];

                _positions.push_back(vx);
                _positions.push_back(vy);
                _positions.push_back(vz);

                _normals.push_back(nx);
                _normals.push_back(ny);
                _normals.push_back(nz);

                //_indices.push_back(index_offset + v);
            }

            index_offset += fv;
        }
    }
}


void ProcessInput(GLFWwindow *window)
{
    float cameraSpeed = 60.0f / 60.0f;
    float cameraRotationSpeed = 5 / 60.0f;

    glm::vec3 r = glm::cross(camDir, glm::vec3(0, 1, 0));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camPos += cameraSpeed * camDir;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camPos -= cameraSpeed * camDir;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camPos -= glm::vec3(0, 1, 0) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camPos += glm::vec3(0, 1, 0) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camDir = glm::rotate(camDir, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camDir = glm::rotate(camDir, -cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camDir = glm::rotate(camDir, cameraRotationSpeed, r);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camDir = glm::rotate(camDir, -cameraRotationSpeed, r);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        lightDir = glm::rotate(lightDir, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        lightDir = glm::rotate(lightDir, cameraRotationSpeed, glm::vec3(1, 0, 0));

    camUp = -glm::cross(r, camDir);
}


void BlitForPresent(VkCommandBuffer cmd, VkImage sourceImage, VkImage swapchainImage, int32_t width, int32_t height)
{
    BarrierImage(cmd, sourceImage,
                 VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    BarrierImage(cmd, swapchainImage,
                 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit region = {};

    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.srcOffsets[0] = { 0, 0, 0 };
    region.srcOffsets[1] = { width, height, 1 };

    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstOffsets[0] = { 0, 0, 0 };
    region.dstOffsets[1] = { width, height, 1 };

    vkCmdBlitImage(cmd, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &region, VK_FILTER_LINEAR);

    BarrierImage(cmd, sourceImage,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    BarrierImage(cmd, swapchainImage,
                 VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}


void Draw(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mainVk.rtPipeline);
    
    VkDescriptorSet sets[] = {
        mainVk.rtDescSets[frameIndex],
        mainVk.storageImageSets[frameIndex],
        rtglData.uniformDescSets[frameIndex],
        rtglData.staticVertsDescSet,
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mainVk.rtPipelineLayout,
                            0, 4, sets, 0, nullptr);

    uint32_t groupCount = mainVk.shaderGroupCount;
    VkDeviceSize sbtAlignment = mainVk.rayTracingProperties.shaderGroupBaseAlignment;
    VkDeviceSize sbtHandleSize = mainVk.rayTracingProperties.shaderGroupHandleSize;
    VkDeviceSize sbtSize = sbtAlignment * groupCount;

    VkStridedBufferRegionKHR raygenEntry = {};
    raygenEntry.buffer = mainVk.shaderBindingTable.buffer;
    raygenEntry.offset = sbtAlignment * (uint32_t) ShaderIndex::RayGen;
    raygenEntry.stride = sbtAlignment;
    raygenEntry.size = sbtSize;

    VkStridedBufferRegionKHR missEntry = {};
    missEntry.buffer = mainVk.shaderBindingTable.buffer;
    missEntry.offset = sbtAlignment * (uint32_t) ShaderIndex::Miss;
    missEntry.stride = sbtAlignment;
    missEntry.size = sbtSize;

    VkStridedBufferRegionKHR hitEntry = {};
    hitEntry.buffer = mainVk.shaderBindingTable.buffer;
    hitEntry.offset = sbtAlignment * (uint32_t) ShaderIndex::ClosestHit;
    hitEntry.stride = sbtAlignment;
    hitEntry.size = sbtSize;

    VkStridedBufferRegionKHR callableEntry = {};

    vksCmdTraceRaysKHR(cmd, &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                      width, height, 1);
}


void main()
{
    LoadModel("../../BRUSHES.obj");
    LoadModel("../../MODELS.obj");

    Window window = {};

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window.glfwHandle = glfwCreateWindow(1600, 900, "Raytracing Test", nullptr, nullptr);
    glfwGetFramebufferSize(window.glfwHandle, (int *) &window.width, (int *) &window.height);
    window.extensions = glfwGetRequiredInstanceExtensions(&window.extensionCount);

    CreateInstance(window);
    glfwCreateWindowSurface(mainVk.instance, window.glfwHandle, NULL, &mainVk.surface);

    CreateDevice();

    CreateCmdPools();
    CreateSyncPrimitives();

    CreateVertexBuffers();
    UploadVertexData();
    CreateInstanceBuffers();
    CreateBottomAS(true);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateTopLevelAS(true, i);
    }

    CreateSwapchain(true, window.width, window.height);
    CreateStorageImage(window.width, window.height);
    CreateUniformBuffer();

    LoadShaders();
    CreateRayTracingDescriptors();
    CreateRayTracingPipeline();
    CreateShaderBindingTable();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateASDescSetBinding(i);
    }

    while (!glfwWindowShouldClose(window.glfwHandle))
    {
        VkResult r;

        glfwPollEvents();
        ProcessInput(window.glfwHandle);

        mainVk.currentFrameIndex = (mainVk.currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

        VkFence frameFence = mainVk.frameFences[mainVk.currentFrameIndex];

        r = vkWaitForFences(mainVk.device, 1, &frameFence, VK_TRUE, UINT64_MAX);
        VK_CHECKERROR(r);

        FrameSemaphores &frameSemaphores = mainVk.frameSemaphores[mainVk.currentFrameIndex];

        while (true)
        {
            r = vkAcquireNextImageKHR(mainVk.device, mainVk.swapchain, UINT64_MAX,
                                      frameSemaphores.imageAvailable,
                                      VK_NULL_HANDLE, &mainVk.currentSwapchainIndex);

            if (r == VK_SUCCESS)
            {
                break;
            }
            else if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            {
                // TODO: recreate swapchain
                assert(0);
            }
            else
            {
                assert(0);
            }
        }

        r = vkResetFences(mainVk.device, 1, &mainVk.frameFences[mainVk.currentFrameIndex]);
        VK_CHECKERROR(r);

        mainVk.frameCmds.graphics[mainVk.currentFrameIndex].Reset();
        mainVk.frameCmds.compute[mainVk.currentFrameIndex].Reset();
        mainVk.frameCmds.transfer[mainVk.currentFrameIndex].Reset();

        FrameCmdBuffers &frameCmds = mainVk.frameCmds.graphics[mainVk.currentFrameIndex];
        VkCommandBuffer cmd = frameCmds.BeginCmd();

        UpdateUniformBuffer();
        Draw(cmd, mainVk.currentFrameIndex, window.width, window.height);

        VkImage outputImage = mainVk.outputImage.image;
        VkImage swapchainImage = mainVk.swapchainImages[mainVk.currentSwapchainIndex];
        BlitForPresent(cmd, outputImage, swapchainImage, window.width, window.height);

        frameCmds.Submit(cmd, 
                         frameSemaphores.imageAvailable, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         frameSemaphores.renderFinished, 
                         frameFence);
        frameCmds.WaitIdle();

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &frameSemaphores.renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mainVk.swapchain;
        presentInfo.pImageIndices = &mainVk.currentSwapchainIndex;
        presentInfo.pResults = nullptr;

        r = vkQueuePresentKHR(mainVk.queues.graphics, &presentInfo);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        {
            // TODO: recreate swapchain
            assert(0);
        }
    }

    DestroyVertexBuffers();

    glfwDestroyWindow(window.glfwHandle);
    glfwTerminate();
}
