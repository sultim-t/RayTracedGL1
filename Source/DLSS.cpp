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

static void PrintCallback( const char*             message,
                           NVSDK_NGX_Logging_Level loggingLevel,
                           NVSDK_NGX_Feature       sourceComponent )
{
    RTGL1::debug::Verbose(
        "DLSS: NVSDK_NGX_Feature={}: {}", static_cast< int >( sourceComponent ), message );
}

RTGL1::DLSS::DLSS( VkInstance       _instance,
                   VkDevice         _device,
                   VkPhysicalDevice _physDevice,
                   const char*      _pAppGuid,
                   bool             _enableDebug )
    : device( _device )
    , isInitialized( false )
    , pParams( nullptr )
    , pDlssFeature( nullptr )
    , prevDlssFeatureValues{}
{
    isInitialized = TryInit( _instance, _device, _physDevice, _pAppGuid, _enableDebug );

    if( !CheckSupport() )
    {
        Destroy();
    }
}

static std::wstring GetFolderPath()
{
    #if defined( _WIN32 )
    wchar_t appPath[ MAX_PATH ];
    GetModuleFileNameW( NULL, appPath, MAX_PATH );
    #elif defined( __linux__ )
    wchar_t appPath[ PATH_MAX ];
    char    appPath_c[ PATH_MAX ];
    ssize_t count = readlink( "/proc/self/exe", appPath_c, PATH_MAX );
    std::mbstowcs( appPath, appPath_c, PATH_MAX );
    #endif

    std::wstring curFolderPath = appPath;
    auto         p             = curFolderPath.find_last_of( L"\\/" );
    return curFolderPath.substr( 0, p );
}

bool RTGL1::DLSS::TryInit( VkInstance       instance,
                           VkDevice         device,
                           VkPhysicalDevice physDevice,
                           const char*      pAppGuid,
                           bool             enableDebug )
{
    NVSDK_NGX_Result r;

    std::wstring dllPath = GetFolderPath() + ( enableDebug ? L"/dev/" : L"/rel/" );
    #ifdef NV_WINDOWS
    wchar_t* dllPath_c = ( wchar_t* )dllPath.c_str();
    #else
    char  dllPath_c_buf[ PATH_MAX ];
    char* dllPath_c = &dllPath_c_buf[ 0 ];
    std::wcstombs( dllPath_c, dllPath.c_str(), PATH_MAX );
    #endif

    NVSDK_NGX_PathListInfo pathsInfo = {
        .Path   = &dllPath_c,
        .Length = 1,
    };

    NGSDK_NGX_LoggingInfo releaseLogInfo = {};
    NGSDK_NGX_LoggingInfo debugLogInfo   = {
          .LoggingCallback     = &PrintCallback,
          .MinimumLoggingLevel = NVSDK_NGX_Logging_Level::NVSDK_NGX_LOGGING_LEVEL_ON,
    };

    NVSDK_NGX_FeatureCommonInfo commonInfo = {
        .PathListInfo = pathsInfo,
        .LoggingInfo  = enableDebug ? debugLogInfo : releaseLogInfo,
    };

    {
        const std::regex guidRegex(
            "^[{]?[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}[}]?$" );

        if( pAppGuid == nullptr )
        {
            throw RgException(
                RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                "Application GUID wasn't provided. Generate and specify it to use DLSS." );
        }

        if( !std::regex_match( pAppGuid, guidRegex ) )
        {
            throw RgException(
                RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                "Provided application GUID is not GUID. Generate and specify correct "
                "GUID to use DLSS." );
        }
    }

    r = NVSDK_NGX_VULKAN_Init_with_ProjectID( pAppGuid,
                                              NVSDK_NGX_EngineType::NVSDK_NGX_ENGINE_TYPE_CUSTOM,
                                              RG_RTGL_VERSION_API,
                                              L"DLSSTemp/",
                                              instance,
                                              physDevice,
                                              device,
                                              &commonInfo );

    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NVSDK_NGX_VULKAN_Init_with_ProjectID fail: {}",
                        static_cast< int >( r ) );
        return false;
    }

    r = NVSDK_NGX_VULKAN_GetCapabilityParameters( &pParams );
    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NVSDK_NGX_VULKAN_GetCapabilityParameters fail: {}",
                        static_cast< int >( r ) );

        NVSDK_NGX_VULKAN_Shutdown();
        pParams = nullptr;

        return false;
    }

    return true;
}

bool RTGL1::DLSS::CheckSupport() const
{
    if( !isInitialized || pParams == nullptr )
    {
        return false;
    }

    int          needsUpdatedDriver    = 0;
    unsigned int minDriverVersionMajor = 0;
    unsigned int minDriverVersionMinor = 0;

    NVSDK_NGX_Result r_upd =
        pParams->Get( NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver );
    NVSDK_NGX_Result r_mjr = pParams->Get( NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor,
                                           &minDriverVersionMajor );
    NVSDK_NGX_Result r_mnr = pParams->Get( NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor,
                                           &minDriverVersionMinor );

    if( NVSDK_NGX_SUCCEED( r_upd ) && NVSDK_NGX_SUCCEED( r_mjr ) && NVSDK_NGX_SUCCEED( r_mnr ) )
    {
        if( needsUpdatedDriver )
        {
            debug::Warning( "DLSS: Can't load: Outdated driver. Min driver version: {}",
                            minDriverVersionMinor );
            return false;
        }
        else
        {
            debug::Verbose( "DLSS: Reported Min driver version: {}", minDriverVersionMinor );
        }
    }
    else
    {
        debug::Warning( "DLSS: Minimum driver version was not reported" );
    }


    int              isDlssSupported = 0;
    NVSDK_NGX_Result featureInitResult;
    NVSDK_NGX_Result r;

    r = pParams->Get( NVSDK_NGX_Parameter_SuperSampling_Available, &isDlssSupported );
    if( NVSDK_NGX_FAILED( r ) || !isDlssSupported )
    {
        // more details about what failed (per feature init result)
        r = NVSDK_NGX_Parameter_GetI( pParams,
                                      NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,
                                      ( int* )&featureInitResult );
        if( NVSDK_NGX_SUCCEED( r ) )
        {
            debug::Warning( "DLSS: Not available on this hardware/platform. FeatureInitResult={}",
                            static_cast< int >( featureInitResult ) );
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
    assert( pDlssFeature != nullptr );

    vkDeviceWaitIdle( device );

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_ReleaseFeature( pDlssFeature );
    pDlssFeature       = nullptr;

    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NVSDK_NGX_VULKAN_ReleaseFeature fail: {}", static_cast< int >( r ) );
    }
}

void RTGL1::DLSS::Destroy()
{
    if( isInitialized )
    {
        vkDeviceWaitIdle( device );

        if( pDlssFeature != nullptr )
        {
            DestroyDlssFeature();
        }

        NVSDK_NGX_VULKAN_DestroyParameters( pParams );
        NVSDK_NGX_VULKAN_Shutdown();

        pParams       = nullptr;
        isInitialized = false;
    }
}

bool RTGL1::DLSS::IsDlssAvailable() const
{
    return isInitialized && pParams != nullptr /* && pDlssFeature != nullptr */;
}

static NVSDK_NGX_PerfQuality_Value ToNGXPerfQuality( RgRenderResolutionMode mode )
{
    switch( mode )
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
            assert( 0 );
            return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_Balanced;
    }
}

bool RTGL1::DLSS::AreSameDlssFeatureValues( const RenderResolutionHelper& renderResolution ) const
{
    return prevDlssFeatureValues.renderWidth == renderResolution.Width() &&
           prevDlssFeatureValues.renderHeight == renderResolution.Height() &&
           prevDlssFeatureValues.upscaledWidth == renderResolution.UpscaledWidth() &&
           prevDlssFeatureValues.upscaledHeight == renderResolution.UpscaledHeight();
}

void RTGL1::DLSS::SaveDlssFeatureValues( const RenderResolutionHelper& renderResolution )
{
    prevDlssFeatureValues = {
        .renderWidth    = renderResolution.Width(),
        .renderHeight   = renderResolution.Height(),
        .upscaledWidth  = renderResolution.UpscaledWidth(),
        .upscaledHeight = renderResolution.UpscaledHeight(),
    };
}

bool RTGL1::DLSS::ValidateDlssFeature( VkCommandBuffer               cmd,
                                       const RenderResolutionHelper& renderResolution )
{
    if( !isInitialized || pParams == nullptr )
    {
        return false;
    }


    if( AreSameDlssFeatureValues( renderResolution ) )
    {
        return true;
    }
    SaveDlssFeatureValues( renderResolution );


    if( pDlssFeature != nullptr )
    {
        DestroyDlssFeature();
    }


    NVSDK_NGX_DLSS_Create_Params dlssParams = {
        .Feature = { .InWidth        = renderResolution.Width(),
                     .InHeight       = renderResolution.Height(),
                     .InTargetWidth  = renderResolution.UpscaledWidth(),
                     .InTargetHeight = renderResolution.UpscaledHeight() },
    };
    // dlssParams.Feature.InPerfQualityValue =
    // ToNGXPerfQuality(renderResolution.GetResolutionMode());

    int& dlssCreateFeatureFlags = dlssParams.InFeatureCreateFlags;
    // motion vectors are in render resolution, not target resolution
    dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    dlssCreateFeatureFlags |= 0; // NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;

    // only one phys device
    uint32_t creationNodeMask   = 1;
    uint32_t visibilityNodeMask = 1;


    NVSDK_NGX_Result r = NGX_VULKAN_CREATE_DLSS_EXT(
        cmd, creationNodeMask, visibilityNodeMask, &pDlssFeature, pParams, &dlssParams );
    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NGX_VULKAN_CREATE_DLSS_EXT fail: {}", static_cast< int >( r ) );

        pDlssFeature = nullptr;
        return false;
    }

    return true;
}

static NVSDK_NGX_Resource_VK ToNGXResource(
    const std::shared_ptr< RTGL1::Framebuffers >& framebuffers,
    uint32_t                                      frameIndex,
    RTGL1::FramebufferImageIndex                  imageIndex,
    NVSDK_NGX_Dimensions                          size,
    bool                                          withWriteAccess = false )
{
    auto [ image, view, format ] = framebuffers->GetImageHandles( imageIndex, frameIndex );

    VkImageSubresourceRange subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    return NVSDK_NGX_Create_ImageView_Resource_VK(
        view, image, subresourceRange, format, size.Width, size.Height, withWriteAccess );
}

RTGL1::FramebufferImageIndex RTGL1::DLSS::Apply(
    VkCommandBuffer                        cmd,
    uint32_t                               frameIndex,
    const std::shared_ptr< Framebuffers >& framebuffers,
    const RenderResolutionHelper&          renderResolution,
    RgFloat2D                              jitterOffset,
    bool                                   resetAccumulation )
{
    if( !IsDlssAvailable() )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "Nvidia DLSS is not supported (or DLSS dynamic library files are not "
                           "found). Check availability before usage." );
    }


    ValidateDlssFeature( cmd, renderResolution );


    if( pDlssFeature == nullptr )
    {
        throw RgException(
            RG_RESULT_GRAPHICS_API_ERROR,
            "Internal error of Nvidia DLSS: NGX_VULKAN_CREATE_DLSS_EXT has failed." );
    }


    using FI             = FramebufferImageIndex;
    const FI outputImage = FI::FB_IMAGE_INDEX_UPSCALED_PONG;


    CmdLabel label( cmd, "DLSS" );


    FI fs[] = {
        FI::FB_IMAGE_INDEX_FINAL,
        FI::FB_IMAGE_INDEX_MOTION_DLSS,
        FI::FB_IMAGE_INDEX_DEPTH_NDC,
    };
    framebuffers->BarrierMultiple( cmd, frameIndex, fs, Framebuffers::BarrierType::Storage );


    NVSDK_NGX_Coordinates sourceOffset = { 0, 0 };
    NVSDK_NGX_Dimensions  sourceSize   = {
        renderResolution.Width(),
        renderResolution.Height(),
    };
    NVSDK_NGX_Dimensions targetSize = {
        renderResolution.UpscaledWidth(),
        renderResolution.UpscaledHeight(),
    };


    // clang-format off
    NVSDK_NGX_Resource_VK unresolvedColorResource = ToNGXResource( framebuffers, frameIndex, FI::FB_IMAGE_INDEX_FINAL, sourceSize );
    NVSDK_NGX_Resource_VK resolvedColorResource   = ToNGXResource( framebuffers, frameIndex, outputImage, targetSize, true );
    NVSDK_NGX_Resource_VK motionVectorsResource   = ToNGXResource( framebuffers, frameIndex, FI::FB_IMAGE_INDEX_MOTION_DLSS, sourceSize );
    NVSDK_NGX_Resource_VK depthResource           = ToNGXResource( framebuffers, frameIndex, FI::FB_IMAGE_INDEX_DEPTH_NDC, sourceSize );
    // clang-format on


    NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {
        .Feature                   = { .pInColor    = &unresolvedColorResource,
                                       .pInOutput   = &resolvedColorResource,
                                       .InSharpness = renderResolution.GetNvDlssSharpness() },
        .pInDepth                  = &depthResource,
        .pInMotionVectors          = &motionVectorsResource,
        .InJitterOffsetX           = jitterOffset.data[ 0 ] * ( -1 ),
        .InJitterOffsetY           = jitterOffset.data[ 1 ] * ( -1 ),
        .InRenderSubrectDimensions = sourceSize,
        .InReset                   = resetAccumulation ? 1 : 0,
        .InMVScaleX                = float( sourceSize.Width ),
        .InMVScaleY                = float( sourceSize.Height ),
        .InColorSubrectBase        = sourceOffset,
        .InDepthSubrectBase        = sourceOffset,
        .InMVSubrectBase           = sourceOffset,
        .InTranslucencySubrectBase = sourceOffset,
    };

    NVSDK_NGX_Result r = NGX_VULKAN_EVALUATE_DLSS_EXT( cmd, pDlssFeature, pParams, &evalParams );

    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NGX_VULKAN_EVALUATE_DLSS_EXT fail: {}", static_cast< int >( r ) );
    }

    return outputImage;
}

void RTGL1::DLSS::GetOptimalSettings( uint32_t               userWidth,
                                      uint32_t               userHeight,
                                      RgRenderResolutionMode mode,
                                      uint32_t*              pOutWidth,
                                      uint32_t*              pOutHeight,
                                      float*                 pOutSharpness ) const
{
    *pOutWidth     = userWidth;
    *pOutHeight    = userHeight;
    *pOutSharpness = 0.0f;

    if( !isInitialized || pParams == nullptr )
    {
        return;
    }

    uint32_t         minWidth, minHeight, maxWidth, maxHeight;
    NVSDK_NGX_Result r = NGX_DLSS_GET_OPTIMAL_SETTINGS( pParams,
                                                        userWidth,
                                                        userHeight,
                                                        ToNGXPerfQuality( mode ),
                                                        pOutWidth,
                                                        pOutHeight,
                                                        &maxWidth,
                                                        &maxHeight,
                                                        &minWidth,
                                                        &minHeight,
                                                        pOutSharpness );
    if( NVSDK_NGX_FAILED( r ) )
    {
        debug::Warning( "DLSS: NGX_DLSS_GET_OPTIMAL_SETTINGS fail: {}", static_cast< int >( r ) );
    }
}

std::vector< const char* > RTGL1::DLSS::GetDlssVulkanInstanceExtensions()
{
    uint32_t     instanceExtCount;
    const char** ppInstanceExts;
    uint32_t     deviceExtCount;
    const char** ppDeviceExts;

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_RequiredExtensions(
        &instanceExtCount, &ppInstanceExts, &deviceExtCount, &ppDeviceExts );
    if( !NVSDK_NGX_SUCCEED( r ) )
    {
        debug::Warning( "DLSS: NVSDK_NGX_VULKAN_RequiredExtensions fail: {}",
                        static_cast< int >( r ) );
    }

    std::vector< const char* > v;

    for( uint32_t i = 0; i < instanceExtCount; i++ )
    {
        v.push_back( ppInstanceExts[ i ] );
    }

    return v;
}

std::vector< const char* > RTGL1::DLSS::GetDlssVulkanDeviceExtensions()
{
    uint32_t     instanceExtCount;
    const char** ppInstanceExts;
    uint32_t     deviceExtCount;
    const char** ppDeviceExts;

    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_RequiredExtensions(
        &instanceExtCount, &ppInstanceExts, &deviceExtCount, &ppDeviceExts );
    if( !NVSDK_NGX_SUCCEED( r ) )
    {
        debug::Warning( "DLSS: NVSDK_NGX_VULKAN_RequiredExtensions fail: {}",
                        static_cast< int >( r ) );
    }

    std::vector< const char* > v;

    for( uint32_t i = 0; i < deviceExtCount; i++ )
    {
        if( strcmp( ppDeviceExts[ i ], VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 )
        {
            continue;
        }

        v.push_back( ppDeviceExts[ i ] );
    }

    return v;
}


#else


RTGL1::DLSS::DLSS( VkInstance       _instance,
                   VkDevice         _device,
                   VkPhysicalDevice _physDevice,
                   const char*      _pAppGuid,
                   bool             _enableDebug )
    : device( _device )
    , isInitialized( false )
    , pParams( nullptr )
    , pDlssFeature( nullptr )
    , prevDlssFeatureValues{}
{
}
RTGL1::DLSS::~DLSS() {}

RTGL1::FramebufferImageIndex RTGL1::DLSS::Apply(
    VkCommandBuffer                        cmd,
    uint32_t                               frameIndex,
    const std::shared_ptr< Framebuffers >& framebuffers,
    const RenderResolutionHelper&          renderResolution,
    RgFloat2D                              jitterOffset,
    bool                                   resetAccumulation )
{
    throw RgException(
        RG_RESULT_WRONG_FUNCTION_ARGUMENT,
        "RTGL1 was built without DLSS support. Enable RG_WITH_NVIDIA_DLSS CMake option." );
}

void RTGL1::DLSS::GetOptimalSettings( uint32_t               userWidth,
                                      uint32_t               userHeight,
                                      RgRenderResolutionMode mode,
                                      uint32_t*              pOutWidth,
                                      uint32_t*              pOutHeight,
                                      float*                 pOutSharpness ) const
{
    throw RgException(
        RG_RESULT_WRONG_FUNCTION_ARGUMENT,
        "RTGL1 was built without DLSS support. Enable RG_WITH_NVIDIA_DLSS CMake option." );
}

bool RTGL1::DLSS::IsDlssAvailable() const
{
    return false;
}

std::vector< const char* > RTGL1::DLSS::GetDlssVulkanInstanceExtensions()
{
    return {};
}
std::vector< const char* > RTGL1::DLSS::GetDlssVulkanDeviceExtensions()
{
    return {};
}


// private
bool RTGL1::DLSS::TryInit( VkInstance       instance,
                           VkDevice         device,
                           VkPhysicalDevice physDevice,
                           const char*      pAppGuid,
                           bool             enableDebug )
{
    return false;
}
bool RTGL1::DLSS::CheckSupport() const
{
    return false;
}
void RTGL1::DLSS::Destroy() {}
void RTGL1::DLSS::DestroyDlssFeature() {}
bool RTGL1::DLSS::AreSameDlssFeatureValues( const RenderResolutionHelper& renderResolution ) const
{
    return false;
}
void RTGL1::DLSS::SaveDlssFeatureValues( const RenderResolutionHelper& renderResolution ) {}
bool RTGL1::DLSS::ValidateDlssFeature( VkCommandBuffer               cmd,
                                       const RenderResolutionHelper& renderResolution )
{
    return false;
}


#endif // RG_USE_NVIDIA_DLSS
