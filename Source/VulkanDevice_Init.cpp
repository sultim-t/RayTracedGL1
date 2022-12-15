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
#include "RenderResolutionHelper.h"
#include "RgException.h"
#include "Generated/ShaderCommonC.h"
#include "LibraryConfig.h"

namespace
{

VkSurfaceKHR GetSurfaceFromUser( VkInstance instance, const RgInstanceCreateInfo& info )
{
    using namespace RTGL1;

    VkSurfaceKHR surface;
    VkResult     r;


#ifdef RG_USE_SURFACE_WIN32
    if( info.pWin32SurfaceInfo != nullptr )
    {
        VkWin32SurfaceCreateInfoKHR win32Info = {
            .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = info.pWin32SurfaceInfo->hinstance,
            .hwnd      = info.pWin32SurfaceInfo->hwnd,
        };

        r = vkCreateWin32SurfaceKHR( instance, &win32Info, nullptr, &surface );
        VK_CHECKERROR( r );

        return surface;
    }
#else
    if( info.pWin32SurfaceInfo != nullptr )
    {
        throw RgException( RG_WRONG_ARGUMENT,
                           "pWin32SurfaceInfo is specified, but the library wasn't built with "
                           "RG_USE_SURFACE_WIN32 option" );
    }
#endif // RG_USE_SURFACE_WIN32


#ifdef RG_USE_SURFACE_METAL
    if( info.pMetalSurfaceCreateInfo != nullptr )
    {
        VkMetalSurfaceCreateInfoEXT metalInfo = {};
        metalInfo.sType                       = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        metalInfo.pLayer                      = info.pMetalSurfaceCreateInfo->pLayer;

        r = vkCreateMetalSurfaceEXT( instance, &metalInfo, nullptr, &surface );
        VK_CHECKERROR( r );

        return surface;
    }
#else
    if( info.pMetalSurfaceCreateInfo != nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "pMetalSurfaceCreateInfo is specified, but the library wasn't built "
                           "with RG_USE_SURFACE_METAL option" );
    }
#endif // RG_USE_SURFACE_METAL


#ifdef RG_USE_SURFACE_WAYLAND
    if( info.pWaylandSurfaceCreateInfo != nullptr )
    {
        VkWaylandSurfaceCreateInfoKHR wlInfo = {};
        wlInfo.sType                         = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        wlInfo.display                       = info.pWaylandSurfaceCreateInfo->display;
        wlInfo.surface                       = info.pWaylandSurfaceCreateInfo->surface;

        r = ( instance, &wlInfo, nullptr, &surface );
        VK_CHECKERROR( r );

        return surface;
    }
#else
    if( info.pWaylandSurfaceCreateInfo != nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "pWaylandSurfaceCreateInfo is specified, but the library wasn't built "
                           "with RG_USE_SURFACE_WAYLAND option" );
    }
#endif // RG_USE_SURFACE_WAYLAND


#ifdef RG_USE_SURFACE_XCB
    if( info.pXcbSurfaceCreateInfo != nullptr )
    {
        VkXcbSurfaceCreateInfoKHR xcbInfo = {};
        xcbInfo.sType                     = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        xcbInfo.connection                = info.pXcbSurfaceCreateInfo->connection;
        xcbInfo.window                    = info.pXcbSurfaceCreateInfo->window;

        r = vkCreateXcbSurfaceKHR( instance, &xcbInfo, nullptr, &surface );
        VK_CHECKERROR( r );

        return surface;
    }
#else
    if( info.pXcbSurfaceCreateInfo != nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "pXcbSurfaceCreateInfo is specified, but the library wasn't built with "
                           "RG_USE_SURFACE_XCB option" );
    }
#endif // RG_USE_SURFACE_XCB


#ifdef RG_USE_SURFACE_XLIB
    if( info.pXlibSurfaceCreateInfo != nullptr )
    {
        VkXlibSurfaceCreateInfoKHR xlibInfo = {};
        xlibInfo.sType                      = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        xlibInfo.dpy                        = info.pXlibSurfaceCreateInfo->dpy;
        xlibInfo.window                     = info.pXlibSurfaceCreateInfo->window;

        r = vkCreateXlibSurfaceKHR( instance, &xlibInfo, nullptr, &surface );
        VK_CHECKERROR( r );

        return surface;
    }
#else
    if( info.pXlibSurfaceCreateInfo != nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "pXlibSurfaceCreateInfo is specified, but the library wasn't built with "
                           "RG_USE_SURFACE_XLIB option" );
    }
#endif // RG_USE_SURFACE_XLIB


    throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Surface info wasn't specified" );
}

} // anonymous namespace

RTGL1::VulkanDevice::VulkanDevice( const RgInstanceCreateInfo* info )
    : instance( VK_NULL_HANDLE )
    , device( VK_NULL_HANDLE )
    , surface( VK_NULL_HANDLE )
    , frameId( 1 )
    , waitForOutOfFrameFence( false )
    , libconfig( LibraryConfig::Read( info->pConfigPath ) )
    , debugMessenger( VK_NULL_HANDLE )
    , userPrint{ std::make_unique< UserPrint >( info->pfnPrint, info->pUserPrintData ) }
    , userFileLoad{ std::make_shared< UserFileLoad >(
          info->pfnOpenFile, info->pfnCloseFile, info->pUserLoadFileData ) }
    , modelsPath( info->pOverridenTexturesFolderPath ? info->pOverridenTexturesFolderPath
                                                     : DEFAULT_MODELS_PATH )
    , rayCullBackFacingTriangles( info->rayCullBackFacingTriangles )
    , allowGeometryWithSkyFlag( info->allowGeometryWithSkyFlag )
    , defaultWorldUp( Utils::SafeNormalize( info->worldUp, { 0, 1, 0 } ) )
    , defaultWorldForward( Utils::SafeNormalize( info->worldForward, { 0, 0, 1 } ) )
    , defaultWorldScale( std::max( info->worldScale, 0.0f ) )
    , previousFrameTime( -1.0 / 60.0 )
    , currentFrameTime( 0 )
    , vsync( true )
{
    ValidateCreateInfo( info );



    // init vulkan instance
    CreateInstance( *info );


    // clang-format off


    // create VkSurfaceKHR using user's function
    surface = GetSurfaceFromUser( instance, *info );


    // create selected physical device
    physDevice = std::make_shared< PhysicalDevice >( instance );
    queues     = std::make_shared< Queues >( physDevice->Get(), surface );

    // create vulkan device and set extension function pointers
    CreateDevice();

    CreateSyncPrimitives();

    // set device
    queues->SetDevice( device );


    memAllocator = std::make_shared< MemoryAllocator >( 
        instance, 
        device, 
        physDevice );

    cmdManager = std::make_shared< CommandBufferManager >( 
        device, 
        queues );

    uniform = std::make_shared< GlobalUniform >( 
        device, 
        memAllocator );

    swapchain = std::make_shared< Swapchain >(
        device, 
        surface, 
        physDevice->Get(), 
        cmdManager );

    if( libconfig.developerMode )
    {
        debugWindows = std::make_shared< DebugWindows >( 
            instance,
            physDevice->Get(),
            device,
            queues->GetIndexGraphics(),
            queues->GetGraphics(),
            cmdManager );
        debugWindows->Init( debugWindows );

        if ( info->pOverridenTexturesFolderPathDeveloper )
        {
            modelsPath = info->pOverridenTexturesFolderPathDeveloper;
        }
    }

    // for world samplers with modifyable lod biad
    worldSamplerManager = std::make_shared< SamplerManager >( device, 8, info->textureSamplerForceMinificationFilterLinear );
    genericSamplerManager = std::make_shared< SamplerManager >( device, 0, info->textureSamplerForceMinificationFilterLinear );

    framebuffers = std::make_shared< Framebuffers >( 
        device, 
        memAllocator, 
        cmdManager,
        *info );

    restirBuffers = std::make_shared< RestirBuffers >( 
        device, 
        memAllocator );

    blueNoise = std::make_shared< BlueNoise >(
        device,
        info->pBlueNoiseFilePath, 
        memAllocator, 
        cmdManager, 
        userFileLoad );

    textureManager = std::make_shared< TextureManager >(
        device, 
        memAllocator, 
        worldSamplerManager,
        cmdManager, 
        userFileLoad,
        *info, 
        libconfig );

    cubemapManager = std::make_shared< CubemapManager >(
        device, 
        memAllocator, 
        genericSamplerManager, 
        cmdManager, 
        userFileLoad, 
        *info, 
        libconfig );

    shaderManager = std::make_shared< ShaderManager >( 
        device, 
        info->pShaderFolderPath,
        userFileLoad );

    scene = std::make_shared< Scene >(
        device, 
        *physDevice,
        memAllocator, 
        cmdManager, 
        textureManager, 
        *uniform, 
        *shaderManager );

    tonemapping = std::make_shared< Tonemapping >(
        device, 
        framebuffers, 
        shaderManager, 
        uniform, 
        memAllocator );

    volumetric = std::make_shared< Volumetric >( 
        device,
        cmdManager.get(),
        memAllocator.get(),
        shaderManager.get(),
        uniform.get(),
        blueNoise.get() );

    rasterizer = std::make_shared< Rasterizer >( 
        device,
        physDevice->Get(),
        *shaderManager,
        textureManager,
        *uniform,
        *genericSamplerManager,
        *tonemapping,
        *volumetric,
        memAllocator,
        framebuffers,
        cmdManager,
        *info );

    decalManager = std::make_shared< DecalManager >(
        device, 
        memAllocator, 
        shaderManager, 
        uniform, 
        framebuffers,
        textureManager );

    portalList = std::make_shared< PortalList >( 
        device,
        memAllocator );

    lightManager = std::make_shared< LightManager >( 
        device, 
        memAllocator );

    lightGrid = std::make_shared< LightGrid >(
        device,
        shaderManager, 
        uniform, 
        blueNoise, 
        lightManager );

    rtPipeline = std::make_shared< RayTracingPipeline >( 
        device,
        physDevice,
        memAllocator,
        *shaderManager,
        *scene,
        *uniform,
        *textureManager,
        *framebuffers,
        *restirBuffers,
        *blueNoise,
        *lightManager,
        *cubemapManager,
        *rasterizer->GetRenderCubemap(),
        *portalList,
        *volumetric,
        *info );

    pathTracer = std::make_shared< PathTracer >( 
        device,
        rtPipeline );

    imageComposition = std::make_shared< ImageComposition >(
        device, 
        memAllocator, 
        framebuffers, 
        *shaderManager, 
        *uniform, 
        *tonemapping,
        *volumetric );

    bloom = std::make_shared< Bloom >( 
        device, 
        framebuffers, 
        shaderManager, 
        uniform, 
        tonemapping );

    amdFsr2 = std::make_shared< FSR2 >( 
        device, 
        physDevice->Get() );

    nvDlss = std::make_shared< DLSS >(
        instance, 
        device, 
        physDevice->Get(), 
        info->pAppGUID, 
        libconfig.dlssValidation );

    sharpening = std::make_shared< Sharpening >( 
        device, 
        framebuffers, 
        shaderManager );

    denoiser = std::make_shared< Denoiser >(
        device, 
        framebuffers, 
        *shaderManager,
        *uniform );

    effectWipe = std::make_shared< EffectWipe >(
        device, 
        framebuffers, 
        uniform, 
        blueNoise, 
        shaderManager, 
        info->effectWipeIsUsed );


    // clang-format on


#define CONSTRUCT_SIMPLE_EFFECT( T ) \
    std::make_shared< T >( device, framebuffers, uniform, shaderManager )
    effectRadialBlur          = CONSTRUCT_SIMPLE_EFFECT( EffectRadialBlur );
    effectChromaticAberration = CONSTRUCT_SIMPLE_EFFECT( EffectChromaticAberration );
    effectInverseBW           = CONSTRUCT_SIMPLE_EFFECT( EffectInverseBW );
    effectHueShift            = CONSTRUCT_SIMPLE_EFFECT( EffectHueShift );
    effectDistortedSides      = CONSTRUCT_SIMPLE_EFFECT( EffectDistortedSides );
    effectWaves               = CONSTRUCT_SIMPLE_EFFECT( EffectWaves );
    effectColorTint           = CONSTRUCT_SIMPLE_EFFECT( EffectColorTint );
    effectCrtDemodulateEncode = CONSTRUCT_SIMPLE_EFFECT( EffectCrtDemodulateEncode );
    effectCrtDecode           = CONSTRUCT_SIMPLE_EFFECT( EffectCrtDecode );
#undef SIMPLE_EFFECT_CONSTRUCTOR_PARAMS


    shaderManager->Subscribe( denoiser );
    shaderManager->Subscribe( imageComposition );
    shaderManager->Subscribe( rasterizer );
    shaderManager->Subscribe( volumetric );
    shaderManager->Subscribe( decalManager );
    shaderManager->Subscribe( rtPipeline );
    shaderManager->Subscribe( lightGrid );
    shaderManager->Subscribe( tonemapping );
    shaderManager->Subscribe( scene->GetVertexPreprocessing() );
    shaderManager->Subscribe( bloom );
    shaderManager->Subscribe( sharpening );
    shaderManager->Subscribe( effectWipe );
    shaderManager->Subscribe( effectRadialBlur );
    shaderManager->Subscribe( effectChromaticAberration );
    shaderManager->Subscribe( effectInverseBW );
    shaderManager->Subscribe( effectHueShift );
    shaderManager->Subscribe( effectDistortedSides );
    shaderManager->Subscribe( effectWaves );
    shaderManager->Subscribe( effectColorTint );
    shaderManager->Subscribe( effectCrtDemodulateEncode );
    shaderManager->Subscribe( effectCrtDecode );

    framebuffers->Subscribe( rasterizer );
    framebuffers->Subscribe( decalManager );
    framebuffers->Subscribe( amdFsr2 );
    framebuffers->Subscribe( restirBuffers );

    // TODO: remove
    scene->StartNewScene( *lightManager );
}

RTGL1::VulkanDevice::~VulkanDevice()
{
    vkDeviceWaitIdle( device );

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
    denoiser.reset();
    uniform.reset();
    scene.reset();
    shaderManager.reset();
    rtPipeline.reset();
    pathTracer.reset();
    rasterizer.reset();
    decalManager.reset();
    portalList.reset();
    lightManager.reset();
    lightGrid.reset();
    worldSamplerManager.reset();
    genericSamplerManager.reset();
    blueNoise.reset();
    textureManager.reset();
    cubemapManager.reset();
    debugWindows.reset();
    memAllocator.reset();

    vkDestroySurfaceKHR( instance, surface, nullptr );
    DestroySyncPrimitives();

    DestroyDevice();
    DestroyInstance();
}

VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugMessengerCallback( VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                            VkDebugUtilsMessageTypeFlagsEXT             messageType,
                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                            void*                                       pUserData )
{
    if( pUserData == nullptr )
    {
        return VK_FALSE;
    }


    // DLSS: ignore error 'VUID-VkCuLaunchInfoNVX-paramCount-arraylength' - 'paramCount must be
    // greater than 0'
    if( pCallbackData->messageIdNumber == 2044605652 )
    {
        return VK_FALSE;
    }


    const char*            msg;
    RgMessageSeverityFlags severity = 0;

    if( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT )
    {
        msg = "Vulkan::VERBOSE::[%d][%s]\n%s\n\n";
        severity |= RG_MESSAGE_SEVERITY_VERBOSE;
    }
    else if( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT )
    {
        msg = "Vulkan::INFO::[%d][%s]\n%s\n\n";
        severity |= RG_MESSAGE_SEVERITY_INFO;
    }
    else if( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
    {
        msg = "Vulkan::WARNING::[%d][%s]\n%s\n\n";
        severity |= RG_MESSAGE_SEVERITY_WARNING;
    }
    else if( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
    {
        msg = "Vulkan::ERROR::[%d][%s]\n%s\n\n";
        severity |= RG_MESSAGE_SEVERITY_ERROR;
    }
    else
    {
        msg = "Vulkan::[%d][%s]\n%s\n\n";
        severity |= RG_MESSAGE_SEVERITY_INFO;
    }

    char buf[ 1024 ];
    snprintf( buf,
              std::size( buf ),
              msg,
              pCallbackData->messageIdNumber,
              pCallbackData->pMessageIdName,
              pCallbackData->pMessage );

    auto* device = static_cast< RTGL1::VulkanDevice* >( pUserData );
    device->Print( buf, severity );

    return VK_FALSE;
}

void RTGL1::VulkanDevice::CreateInstance( const RgInstanceCreateInfo& info )
{
    std::vector< const char* > layerNames;

    if( libconfig.vulkanValidation )
    {
        layerNames.push_back( "VK_LAYER_KHRONOS_validation" );
    }

    if( libconfig.fpsMonitor )
    {
        layerNames.push_back( "VK_LAYER_LUNARG_monitor" );
    }

    std::vector< VkExtensionProperties > supportedInstanceExtensions;
    {
        uint32_t count = 0;
        if( vkEnumerateInstanceExtensionProperties( nullptr, &count, nullptr ) == VK_SUCCESS )
        {
            supportedInstanceExtensions.resize( count );
            vkEnumerateInstanceExtensionProperties(
                nullptr, &count, supportedInstanceExtensions.data() );
        }
    }

    std::vector extensions = {
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

    if( libconfig.vulkanValidation )
    {
        extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        extensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
    }

    for( const char* n : DLSS::GetDlssVulkanInstanceExtensions() )
    {
        const bool isSupported =
            std::ranges::any_of( std::as_const( supportedInstanceExtensions ),
                                 [ n ]( const VkExtensionProperties& ext ) {
                                     return std::strcmp( ext.extensionName, n ) == 0;
                                 } );

        if( !isSupported )
        {
            continue;
        }

        extensions.push_back( n );
    }


    VkApplicationInfo appInfo = {
        .sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = info.pAppName,
        .pEngineName      = "RTGL1",
        .apiVersion       = VK_API_VERSION_1_2,
    };

    VkInstanceCreateInfo instanceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast< uint32_t >( layerNames.size() ),
        .ppEnabledLayerNames     = layerNames.data(),
        .enabledExtensionCount   = static_cast< uint32_t >( extensions.size() ),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkResult r = vkCreateInstance( &instanceInfo, nullptr, &instance );
    VK_CHECKERROR( r );


    if( libconfig.vulkanValidation )
    {
        InitInstanceExtensionFunctions_DebugUtils( instance );
        
        // init debug utilsdebugMessenger
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugMessengerCallback,
            .pUserData       = static_cast< void* >( this ),
        };

        r = svkCreateDebugUtilsMessengerEXT(
            instance, &debugMessengerInfo, nullptr, &debugMessenger );
        VK_CHECKERROR( r );
    }
}

void RTGL1::VulkanDevice::CreateDevice()
{
    VkPhysicalDeviceFeatures features = {
        .robustBufferAccess                      = 1,
        .fullDrawIndexUint32                     = 1,
        .imageCubeArray                          = 1,
        .independentBlend                        = 1,
        .geometryShader                          = 0,
        .tessellationShader                      = 0,
        .sampleRateShading                       = 0,
        .dualSrcBlend                            = 0,
        .logicOp                                 = 1,
        .multiDrawIndirect                       = 1,
        .drawIndirectFirstInstance               = 1,
        .depthClamp                              = 1,
        .depthBiasClamp                          = 1,
        .fillModeNonSolid                        = 0,
        .depthBounds                             = 1,
        .wideLines                               = 0,
        .largePoints                             = 0,
        .alphaToOne                              = 0,
        .multiViewport                           = 1,
        .samplerAnisotropy                       = 1,
        .textureCompressionETC2                  = 0,
        .textureCompressionASTC_LDR              = 0,
        .textureCompressionBC                    = 0,
        .occlusionQueryPrecise                   = 0,
        .pipelineStatisticsQuery                 = 1,
        .vertexPipelineStoresAndAtomics          = 1,
        .fragmentStoresAndAtomics                = 1,
        .shaderTessellationAndGeometryPointSize  = 1,
        .shaderImageGatherExtended               = 1,
        .shaderStorageImageExtendedFormats       = 1,
        .shaderStorageImageMultisample           = 1,
        .shaderStorageImageReadWithoutFormat     = 1,
        .shaderStorageImageWriteWithoutFormat    = 1,
        .shaderUniformBufferArrayDynamicIndexing = 1,
        .shaderSampledImageArrayDynamicIndexing  = 1,
        .shaderStorageBufferArrayDynamicIndexing = 1,
        .shaderStorageImageArrayDynamicIndexing  = 1,
        .shaderClipDistance                      = 1,
        .shaderCullDistance                      = 1,
        .shaderFloat64                           = 1,
        .shaderInt64                             = 1,
        .shaderInt16                             = 1,
        .shaderResourceResidency                 = 1,
        .shaderResourceMinLod                    = 1,
        .sparseBinding                           = 0,
        .sparseResidencyBuffer                   = 0,
        .sparseResidencyImage2D                  = 0,
        .sparseResidencyImage3D                  = 0,
        .sparseResidency2Samples                 = 0,
        .sparseResidency4Samples                 = 0,
        .sparseResidency8Samples                 = 0,
        .sparseResidency16Samples                = 0,
        .sparseResidencyAliased                  = 0,
        .variableMultisampleRate                 = 0,
        .inheritedQueries                        = 1,
    };

    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        .sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .samplerMirrorClampToEdge = 1,
        .drawIndirectCount        = 1,
        .shaderFloat16            = 1,
        .shaderSampledImageArrayNonUniformIndexing  = 1,
        .shaderStorageBufferArrayNonUniformIndexing = 1,
        .runtimeDescriptorArray                     = 1,
        .bufferDeviceAddress                        = 1,
    };

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
        .sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext     = &vulkan12Features,
        .multiview = 1,
    };

    VkPhysicalDevice16BitStorageFeatures storage16 = {
        .sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
        .pNext                    = &multiviewFeatures,
        .storageBuffer16BitAccess = 1,
    };
    VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .pNext            = &storage16,
        .synchronization2 = 1,
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
        .sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext              = &sync2Features,
        .rayTracingPipeline = 1,
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rtPipelineFeatures,
        .accelerationStructure = 1,
    };

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {
        .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext    = &asFeatures,
        .features = features,
    };


    std::vector< VkExtensionProperties > supportedDeviceExtensions;
    {
        uint32_t count = 0;
        if( vkEnumerateDeviceExtensionProperties( physDevice->Get(), nullptr, &count, nullptr ) ==
            VK_SUCCESS )
        {
            supportedDeviceExtensions.resize( count );
            vkEnumerateDeviceExtensionProperties(
                physDevice->Get(), nullptr, &count, supportedDeviceExtensions.data() );
        }
    }

    std::vector deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
    };

    for( const char* n : DLSS::GetDlssVulkanDeviceExtensions() )
    {
        const bool isSupported = std::any_of( supportedDeviceExtensions.cbegin(),
                                              supportedDeviceExtensions.cend(),
                                              [ & ]( const VkExtensionProperties& ext ) {
                                                  return !std::strcmp( ext.extensionName, n );
                                              } );

        if( !isSupported )
        {
            continue;
        }

        deviceExtensions.push_back( n );
    }

    const auto queueCreateInfos = queues->GetDeviceQueueCreateInfos();

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &physicalDeviceFeatures2,
        .queueCreateInfoCount    = static_cast< uint32_t >( queueCreateInfos.size() ),
        .pQueueCreateInfos       = queueCreateInfos.data(),
        .enabledExtensionCount   = static_cast< uint32_t >( deviceExtensions.size() ),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures        = nullptr,
    };

    VkResult r = vkCreateDevice( physDevice->Get(), &deviceCreateInfo, nullptr, &device );
    VK_CHECKERROR( r );

    InitDeviceExtensionFunctions( device );

    if( libconfig.vulkanValidation )
    {
        InitDeviceExtensionFunctions_DebugUtils( device );
    }
}

void RTGL1::VulkanDevice::CreateSyncPrimitives()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        {
            VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            VkResult r = vkCreateSemaphore(
                device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            imageAvailableSemaphores[ i ],
                            VK_OBJECT_TYPE_SEMAPHORE,
                            "Image available semaphore" );
        }

        {
            VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            VkResult r = vkCreateSemaphore(
                device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device,
                            renderFinishedSemaphores[ i ],
                            VK_OBJECT_TYPE_SEMAPHORE,
                            "Render finished semaphore" );
        }

        {
            VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            VkResult r =
                vkCreateSemaphore( device, &semaphoreInfo, nullptr, &inFrameSemaphores[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME(
                device, inFrameSemaphores[ i ], VK_OBJECT_TYPE_SEMAPHORE, "In-frame semaphore" );
        }

        {
            VkFenceCreateInfo signaledFenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
            VkResult r = vkCreateFence( device, &signaledFenceInfo, nullptr, &frameFences[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device, frameFences[ i ], VK_OBJECT_TYPE_FENCE, "Frame fence" );
        }

        {
            VkFenceCreateInfo nonSignaledFenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            };
            VkResult r =
                vkCreateFence( device, &nonSignaledFenceInfo, nullptr, &outOfFrameFences[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME(
                device, outOfFrameFences[ i ], VK_OBJECT_TYPE_FENCE, "Out of frame fence" );
        }
    }
}

void RTGL1::VulkanDevice::DestroyInstance()
{
    if( debugMessenger != VK_NULL_HANDLE )
    {
        svkDestroyDebugUtilsMessengerEXT( instance, debugMessenger, nullptr );
    }

    vkDestroyInstance( instance, nullptr );
}

void RTGL1::VulkanDevice::DestroyDevice()
{
    vkDestroyDevice( device, nullptr );
}

void RTGL1::VulkanDevice::DestroySyncPrimitives()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        vkDestroySemaphore( device, imageAvailableSemaphores[ i ], nullptr );
        vkDestroySemaphore( device, renderFinishedSemaphores[ i ], nullptr );
        vkDestroySemaphore( device, inFrameSemaphores[ i ], nullptr );

        vkDestroyFence( device, frameFences[ i ], nullptr );
        vkDestroyFence( device, outOfFrameFences[ i ], nullptr );
    }
}

void RTGL1::VulkanDevice::ValidateCreateInfo( const RgInstanceCreateInfo* pInfo ) const
{
    using namespace std::string_literals;

    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    {
        int count = !!pInfo->pWin32SurfaceInfo + !!pInfo->pMetalSurfaceCreateInfo +
                    !!pInfo->pWaylandSurfaceCreateInfo + !!pInfo->pXcbSurfaceCreateInfo +
                    !!pInfo->pXlibSurfaceCreateInfo;

        if( count != 1 )
        {
            throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                               "Exactly one of the surface infos must be not null" );
        }
    }

    if( pInfo->rasterizedSkyCubemapSize == 0 )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "rasterizedSkyCubemapSize must be non-zero" );
    }

    if( pInfo->primaryRaysMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "primaryRaysMaxAlbedoLayers must be <="s +
                               std::to_string( MATERIALS_MAX_LAYER_COUNT ) );
    }

    if( pInfo->indirectIlluminationMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "indirectIlluminationMaxAlbedoLayers must be <="s +
                               std::to_string( MATERIALS_MAX_LAYER_COUNT ) );
    }

    if( pInfo->worldScale <= 0.00001f )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "worldScale is too small" );
    }

    if( Utils::IsAlmostZero( pInfo->worldUp ) )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "worldUp vector is too small to represent direction" );
    }

    if( Utils::IsAlmostZero( pInfo->worldForward ) )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                           "worldForward vector is too small to represent direction" );
    }
}
