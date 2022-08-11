// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "DLSS.h"

#include <regex>

#include "RTGL1/RTGL1.h"
#include "CmdLabel.h"
#include "RenderResolutionHelper.h"


#ifdef RG_USE_NVIDIA_DLSS


#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_helpers.h>

#if __linux__
#include <unistd.h>         
#include <linux/limits.h>
#endif

static void PrintCallback(const char *message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
    printf("DLSS (sourceComponent = %d): %s \n", sourceComponent,  message);
}

// TODO: DLSS: add LOG INFO / ERROR

RTGL1::DLSS::DLSS(
    VkInstance _instance,
    VkDevice _device,
    VkPhysicalDevice _physDevice, 
    const char *_pAppGuid,
    bool _enableDebug)
:
    device(_device),
    isInitialized(false),
    pParams(nullptr),
    pDlssFeature(nullptr),
    prevDlssFeatureValues{}
{
    isInitialized = TryInit(_instance, _device, _physDevice, _pAppGuid, _enableDebug);

    if (!CheckSupport())
    {
        Destroy();
    }
}

static std::wstring GetFolderPath()
{
#if defined(_WIN32)
    wchar_t appPath[MAX_PATH];
    GetModuleFileNameW(NULL, appPath, MAX_PATH);
#elif defined(__linux__)
    wchar_t appPath[PATH_MAX];
    char appPath_c[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", appPath_c, PATH_MAX);
    std::mbstowcs(appPath, appPath_c, PATH_MAX);
#endif

    std::wstring curFolderPath = appPath;
    auto p = curFolderPath.find_last_of(L"\\/");
    return curFolderPath.substr(0, p);
}

bool RTGL1::DLSS::TryInit(VkInstance instance, VkDevice device, VkPhysicalDevice physDevice, const char *pAppGuid, bool enableDebug)
{
    NVSDK_NGX_Result r;

    std::wstring dllPath = GetFolderPath() + (enableDebug ? L"/dev/" : L"/rel/") ;
#ifdef NV_WINDOWS
    wchar_t *dllPath_c = (wchar_t *)dllPath.c_str();
#else
    char dllPath_c_buf[PATH_MAX];
    char *dllPath_c = &dllPath_c_buf[0];
    std::wcstombs(dllPath_c, dllPath.c_str(), PATH_MAX);
#endif

    NVSDK_NGX_PathListInfo pathsInfo = {};
    pathsInfo.Path = &dllPath_c;
    pathsInfo.Length = 1;

    NGSDK_NGX_LoggingInfo debugLogInfo = {};
    debugLogInfo.LoggingCallback = &PrintCallback;
    debugLogInfo.MinimumLoggingLevel = NVSDK_NGX_Logging_Level::NVSDK_NGX_LOGGING_LEVEL_ON;

    NGSDK_NGX_LoggingInfo releaseLogInfo = {};

    NVSDK_NGX_FeatureCommonInfo commonInfo = {};
    commonInfo.PathListInfo = pathsInfo;
    commonInfo.LoggingInfo = enableDebug ? debugLogInfo : releaseLogInfo;

    const std::regex guidRegex("^[{]?[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}[}]?$");

    if (pAppGuid == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Application GUID wasn't provided. Generate and specify it to use DLSS.");
    }

    if (!std::regex_match(pAppGuid, guidRegex))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Provided application GUID is not GUID. Generate and specify correct GUID to use DLSS.");
    }

    r = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        pAppGuid,
        NVSDK_NGX_EngineType::NVSDK_NGX_ENGINE_TYPE_CUSTOM, RG_RTGL_VERSION_API, L"DLSSTemp/", instance, physDevice, device, &commonInfo);

    if (NVSDK_NGX_FAILED(r))
    {
        return false;
    }

    r = NVSDK_NGX_VULKAN_GetCapabilityParameters(&pParams);
    if (NVSDK_NGX_FAILED(r))
    {
        NVSDK_NGX_VULKAN_Shutdown();
        pParams = nullptr;
        
        return false;
    }

    return true;
}

bool RTGL1::DLSS::CheckSupport() const
{
    if (!isInitialized || pParams == nullptr)
    {
        return false;
    }

    int needsUpdatedDriver = 0;
    unsigned int minDriverVersionMajor = 0;
    unsigned int minDriverVersionMinor = 0;

    NVSDK_NGX_Result r_upd = pParams->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
    NVSDK_NGX_Result r_mjr = pParams->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVersionMajor);
    NVSDK_NGX_Result r_mnr = pParams->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVersionMinor);

    if (NVSDK_NGX_SUCCEED(r_upd) && NVSDK_NGX_SUCCEED(r_mjr) && NVSDK_NGX_SUCCEED(r_mnr))
    {
        if (needsUpdatedDriver)
        {
            // LOG ERROR("NVIDIA DLSS cannot be loaded due to outdated driver.
            //            Min Driver Version required : minDriverVersionMajor.minDriverVersionMinor");
            return false;
        }
        else
        {
            // LOG INFO("NIDIA DLSS Minimum driver version was reported as: minDriverVersionMajor.minDriverVersionMinor");
        }
    }
    else
    {
        // LOG INFO("NVIDIA DLSS Minimum driver version was not reported.");
    }


    int isDlssSupported = 0;
    NVSDK_NGX_Result featureInitResult;
    NVSDK_NGX_Result r;
    
    r = pParams->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &isDlssSupported);
    if (NVSDK_NGX_FAILED(r) || !isDlssSupported)
    {
        // more details about what failed (per feature init result)
        r = NVSDK_NGX_Parameter_GetI(pParams, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int *)&featureInitResult);
        if (NVSDK_NGX_SUCCEED(r))
        {
            // LOG INFO("NVIDIA DLSS not available on this hardward/platform., FeatureInitResult = 0x%08x, info: %ls\n", featureInitResult, GetNGXResultAsString(featureInitResult));
        }

        return false;
    }

    return true;
}

RTGL1::DLSS::~DLSS()
{
    Destroy();
}

void RTGL1::DLSS::DestroyDlssFeature()
{
    assert(pDlssFeature != nullptr);

    vkDeviceWaitIdle(device);

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_ReleaseFeature(pDlssFeature);
    pDlssFeature = nullptr;

    if (NVSDK_NGX_FAILED(r))
    {
        // LOG ERROR("Failed to NVSDK_NGX_VULKAN_ReleaseFeature, code = 0x%08x, info: %ls", r, GetNGXResultAsString(t));
    }
}

void RTGL1::DLSS::Destroy()
{
    if (isInitialized)
    {
        vkDeviceWaitIdle(device);

        if (pDlssFeature != nullptr)
        {
            DestroyDlssFeature();
        }

        NVSDK_NGX_VULKAN_DestroyParameters(pParams);
        NVSDK_NGX_VULKAN_Shutdown();

        pParams = nullptr;
        isInitialized = false;
    }
}

bool RTGL1::DLSS::IsDlssAvailable() const
{
    return isInitialized && pParams != nullptr /* && pDlssFeature != nullptr */;
}

static NVSDK_NGX_PerfQuality_Value ToNGXPerfQuality(RgRenderResolutionMode mode)
{
    switch (mode)
    {
        case RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE:
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_UltraPerformance;
        case RG_RENDER_RESOLUTION_MODE_PERFORMANCE:
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_MaxPerf;
        case RG_RENDER_RESOLUTION_MODE_BALANCED:
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_Balanced;
        case RG_RENDER_RESOLUTION_MODE_QUALITY:
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_MaxQuality;
        case RG_RENDER_RESOLUTION_MODE_ULTRA_QUALITY:
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_UltraQuality;
        default:
            assert(0); 
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_Balanced;
    }
}

bool RTGL1::DLSS::AreSameDlssFeatureValues(const RenderResolutionHelper &renderResolution) const
{
    return  
        prevDlssFeatureValues.renderWidth    == renderResolution.Width() &&
        prevDlssFeatureValues.renderHeight   == renderResolution.Height() &&
        prevDlssFeatureValues.upscaledWidth  == renderResolution.UpscaledWidth() &&
        prevDlssFeatureValues.upscaledHeight == renderResolution.UpscaledHeight();
}

void RTGL1::DLSS::SaveDlssFeatureValues(const RenderResolutionHelper &renderResolution)
{
    prevDlssFeatureValues.renderWidth = renderResolution.Width();
    prevDlssFeatureValues.renderHeight = renderResolution.Height();
    prevDlssFeatureValues.upscaledWidth = renderResolution.UpscaledWidth();
    prevDlssFeatureValues.upscaledHeight = renderResolution.UpscaledHeight();
}

bool RTGL1::DLSS::ValidateDlssFeature(VkCommandBuffer cmd, const RenderResolutionHelper &renderResolution)
{
    if (!isInitialized || pParams == nullptr)
    {
        return false;
    }


    if (AreSameDlssFeatureValues(renderResolution))
    {
        return true;
    }
    SaveDlssFeatureValues(renderResolution);


    if (pDlssFeature != nullptr)
    {
        DestroyDlssFeature();
    }


    NVSDK_NGX_DLSS_Create_Params dlssParams = {};
    dlssParams.Feature.InWidth = renderResolution.Width();
    dlssParams.Feature.InHeight = renderResolution.Height();
    dlssParams.Feature.InTargetWidth = renderResolution.UpscaledWidth();
    dlssParams.Feature.InTargetHeight = renderResolution.UpscaledHeight();
    // dlssParams.Feature.InPerfQualityValue = ToNGXPerfQuality(renderResolution.GetResolutionMode());

    int &dlssCreateFeatureFlags = dlssParams.InFeatureCreateFlags;
    // motion vectors are in render resolution, not target resolution
    dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;

    // only one phys device
    uint32_t creationNodeMask = 1;
    uint32_t visibilityNodeMask = 1;


    NVSDK_NGX_Result r = NGX_VULKAN_CREATE_DLSS_EXT(cmd, creationNodeMask, visibilityNodeMask,
                                                    &pDlssFeature, pParams, &dlssParams);
    if (NVSDK_NGX_FAILED(r))
    {
        // LOG ERROR("Failed to create DLSS Features = 0x%08x, info: %ls", r, GetNGXResultAsString(r));
     
        pDlssFeature = nullptr;
        return false;
    }

    return true;
}

static NVSDK_NGX_Resource_VK ToNGXResource(const std::shared_ptr<RTGL1::Framebuffers> &framebuffers, uint32_t frameIndex,
                                           RTGL1::FramebufferImageIndex imageIndex, NVSDK_NGX_Dimensions size, bool withWriteAccess = false)
{
    auto [image, view, format] = framebuffers->GetImageHandles(imageIndex, frameIndex);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    return NVSDK_NGX_Create_ImageView_Resource_VK(view, image, subresourceRange, format, size.Width, size.Height, withWriteAccess);
}

RTGL1::FramebufferImageIndex RTGL1::DLSS::Apply(VkCommandBuffer cmd, uint32_t frameIndex,
                                                const std::shared_ptr<Framebuffers> &framebuffers,
                                                const RenderResolutionHelper &renderResolution, 
                                                RgFloat2D jitterOffset)
{
    if (!IsDlssAvailable())
    {
        throw RgException(RG_WRONG_ARGUMENT, "Nvidia DLSS is not supported (or DLSS dynamic library files are not found). Check availability before usage.");
    }


    ValidateDlssFeature(cmd, renderResolution);


    if (pDlssFeature == nullptr)
    {
        throw RgException(RG_GRAPHICS_API_ERROR, "Internal error of Nvidia DLSS: NGX_VULKAN_CREATE_DLSS_EXT has failed.");
    }


    typedef FramebufferImageIndex FI;
    const FI outputImage = FI::FB_IMAGE_INDEX_UPSCALED_PONG;


    CmdLabel label(cmd, "DLSS");


    // TODO: DLSS: resettable accumulation
    //             offset of the viewport render
    int resetAccumulation = 0;
    NVSDK_NGX_Coordinates sourceOffset = { 0, 0 };
    NVSDK_NGX_Dimensions  sourceSize = { renderResolution.Width(),         renderResolution.Height()          };
    NVSDK_NGX_Dimensions  targetSize = { renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight()  };


    NVSDK_NGX_Resource_VK unresolvedColorResource   = ToNGXResource(framebuffers, frameIndex, FI::FB_IMAGE_INDEX_FINAL,       sourceSize);
    NVSDK_NGX_Resource_VK resolvedColorResource     = ToNGXResource(framebuffers, frameIndex, outputImage,                    targetSize, true);
    NVSDK_NGX_Resource_VK motionVectorsResource     = ToNGXResource(framebuffers, frameIndex, FI::FB_IMAGE_INDEX_MOTION_DLSS, sourceSize);
    NVSDK_NGX_Resource_VK depthResource             = ToNGXResource(framebuffers, frameIndex, FI::FB_IMAGE_INDEX_DEPTH_NDC,   sourceSize);


    NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {};
    evalParams.Feature.pInColor = &unresolvedColorResource;
    evalParams.Feature.pInOutput = &resolvedColorResource;
    evalParams.pInDepth = &depthResource;
    evalParams.pInMotionVectors = &motionVectorsResource;
    evalParams.InJitterOffsetX = jitterOffset.data[0] * (-1);
    evalParams.InJitterOffsetY = jitterOffset.data[1] * (-1);
    evalParams.Feature.InSharpness = renderResolution.GetNvDlssSharpness();
    evalParams.InReset = resetAccumulation;
    evalParams.InMVScaleX = static_cast<float>(sourceSize.Width);
    evalParams.InMVScaleY = static_cast<float>(sourceSize.Height);
    evalParams.InColorSubrectBase = sourceOffset;
    evalParams.InDepthSubrectBase = sourceOffset;
    evalParams.InTranslucencySubrectBase = sourceOffset;
    evalParams.InMVSubrectBase = sourceOffset;
    evalParams.InRenderSubrectDimensions = sourceSize;


    NVSDK_NGX_Result r = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd, pDlssFeature, pParams, &evalParams);

    if (NVSDK_NGX_FAILED(r))
    {
       // LOG ERROR("Failed to NVSDK_NGX_VULKAN_EvaluateFeature for DLSS, code = 0x%08x, info: %ls", r, GetNGXResultAsString(r));
    }

    return outputImage;
}

void RTGL1::DLSS::GetOptimalSettings(uint32_t userWidth, uint32_t userHeight, RgRenderResolutionMode mode,
                                     uint32_t *pOutWidth, uint32_t *pOutHeight, float *pOutSharpness) const
{
    *pOutWidth = userWidth;
    *pOutHeight = userHeight;
    *pOutSharpness = 0.0f;

    if (!isInitialized || pParams == nullptr)
    {
        return;
    }

    uint32_t minWidth, minHeight, maxWidth, maxHeight;
    NVSDK_NGX_Result r = NGX_DLSS_GET_OPTIMAL_SETTINGS(pParams,
                                                       userWidth, userHeight, ToNGXPerfQuality(mode),
                                                       pOutWidth, pOutHeight,
                                                       &maxWidth, &maxHeight, &minWidth, &minHeight,
                                                       pOutSharpness);
    if (NVSDK_NGX_FAILED(r))
    {
        // LOG INFO("Querying Optimal Settings failed! code = 0x%08x, info: %ls", r, GetNGXResultAsString(r));
    }
}

std::vector<const char *> RTGL1::DLSS::GetDlssVulkanInstanceExtensions()
{
    uint32_t instanceExtCount;
    const char **ppInstanceExts;
    uint32_t deviceExtCount;
    const char **ppDeviceExts;

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_RequiredExtensions(&instanceExtCount, &ppInstanceExts, &deviceExtCount, &ppDeviceExts);
    assert(NVSDK_NGX_SUCCEED(r));

    std::vector<const char *> v;

    for (uint32_t i = 0; i < instanceExtCount; i++)
    {
        v.push_back(ppInstanceExts[i]);
    }

    return v;
}

std::vector<const char *> RTGL1::DLSS::GetDlssVulkanDeviceExtensions()
{
    uint32_t instanceExtCount;
    const char **ppInstanceExts;
    uint32_t deviceExtCount;
    const char **ppDeviceExts;

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_RequiredExtensions(&instanceExtCount, &ppInstanceExts, &deviceExtCount, &ppDeviceExts);
    assert(NVSDK_NGX_SUCCEED(r));

    std::vector<const char *> v;

    for (uint32_t i = 0; i < deviceExtCount; i++)
    {
        if (strcmp(ppDeviceExts[i], VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0)
        {
            continue;
        }

        v.push_back(ppDeviceExts[i]);
    }

    return v;
}


#else 


RTGL1::DLSS::DLSS(VkInstance _instance, VkDevice _device, VkPhysicalDevice _physDevice, const char *pAppGuid, bool _enableDebug) : device(_device),isInitialized(false), pParams(nullptr), pDlssFeature(nullptr), prevDlssFeatureValues{} { }
RTGL1::DLSS::~DLSS() { }

RTGL1::FramebufferImageIndex RTGL1::DLSS::Apply(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<Framebuffers> &framebuffers, const RenderResolutionHelper &renderResolution, RgFloat2D jitterOffset)
{ throw RgException(RG_WRONG_ARGUMENT, "RTGL1 was built without DLSS support. Enable RG_WITH_NVIDIA_DLSS CMake option."); }

void RTGL1::DLSS::GetOptimalSettings(uint32_t userWidth, uint32_t userHeight, RgRenderResolutionMode mode, uint32_t *pOutWidth, uint32_t *pOutHeight, float *pOutSharpness) const
{ throw RgException(RG_WRONG_ARGUMENT, "RTGL1 was built without DLSS support. Enable RG_WITH_NVIDIA_DLSS CMake option."); }

bool RTGL1::DLSS::IsDlssAvailable() const { return false; }

std::vector<const char *> RTGL1::DLSS::GetDlssVulkanInstanceExtensions() { return {}; }
std::vector<const char *> RTGL1::DLSS::GetDlssVulkanDeviceExtensions() { return {}; }


// private
bool RTGL1::DLSS::TryInit(VkInstance instance, VkDevice device, VkPhysicalDevice physDevice, const char *pAppGuid, bool enableDebug) { return false; }
bool RTGL1::DLSS::CheckSupport() const { return false; }
void RTGL1::DLSS::Destroy() { }
void RTGL1::DLSS::DestroyDlssFeature() { }
bool RTGL1::DLSS::AreSameDlssFeatureValues(const RenderResolutionHelper &renderResolution) const { return false; }
void RTGL1::DLSS::SaveDlssFeatureValues(const RenderResolutionHelper &renderResolution) { }
bool RTGL1::DLSS::ValidateDlssFeature(VkCommandBuffer cmd, const RenderResolutionHelper &renderResolution) { return false; }


#endif // RG_USE_NVIDIA_DLSS
