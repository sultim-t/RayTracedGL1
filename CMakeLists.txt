cmake_minimum_required(VERSION 3.15)
project(RayTracedGL1 CXX C)

set(CMAKE_CXX_STANDARD 20)

# Vulkan
message(STATUS "Adding Vulkan. VulkanSDK: $ENV{VULKAN_SDK}")
find_package(Vulkan REQUIRED)
add_library(Vulkan INTERFACE)
target_include_directories(Vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Vulkan INTERFACE ${Vulkan_LIBRARIES})

set(PublicHeaders 
    Include/RTGL1/RTGL1.h
    Include/RTGL1/RTGL1A.h)

set(PrivateHeaders 
    "Source/Common.h"
    "Source/Const.h"
    "Source/ISwapchainDependency.h"
    "Source/VertexBufferProperties.h"
    "Source/VulkanDevice.h"
    "Source/Buffer.h"
    "Source/Scene.h"
    "Source/PhysicalDevice.h"
    "Source/Queues.h"
    "Source/Swapchain.h"
    "Source/GlobalUniform.h"
    "Source/CommandBufferManager.h"
    "Source/ShaderManager.h"
    "Source/RayTracingPipeline.h"
    "Source/VertexCollector.h"
    "Source/ASManager.h"
    "Source/VertexCollectorFilter.h"
    "Source/ASBuilder.h"
    "Source/ScratchBuffer.h"
    "Source/Utils.h"
    "Source/PathTracer.h"
    "Source/Matrix.h"
    "Source/Rasterizer.h"
    "Source/RasterizedDataCollector.h"
    "Source/ImageLoader.h" 
    "Source/TextureManager.h" 
    "Source/MemoryAllocator.h" 
    "Source/SamplerManager.h" 
    "Source/TextureOverrides.h" 
    "Source/Material.h"
    "Source/TextureDescriptors.h"
    "Source/TextureUploader.h"
    "Source/IMaterialDependency.h"
    "Source/Generated/ShaderCommonC.h"
    "Source/Generated/ShaderCommonCFramebuf.h"
    "Source/Framebuffers.h"
    "Source/BlueNoise.h"
    "Source/ImageComposition.h"
    "Source/Tonemapping.h"
    "Source/LightManager.h"
    "Source/AutoBuffer.h"
    "Source/ASComponent.h"
    "Source/CubemapManager.h"
    "Source/CubemapUploader.h"
    "Source/GeomInfoManager.h"
    "Source/VertexPreprocessing.h"
    "Source/Denoiser.h"
    "Source/IShaderDependency.h"
    "Source/IFramebuffersDependency.h"
    "Source/RasterizerPipelines.h"
    "Source/RenderCubemap.h"
    "Source/DepthCopying.h"
    "Source/RasterPass.h"
    "Source/SwapchainPass.h"
    "Source/UserFunction.h"
    "Source/RgException.h"
    "Source/CmdLabel.h"
    "Source/Bloom.h"
    "Source/SuperResolution.h"
    "Source/Sharpening.h"
    "Source/DLSS.h"
    "Source/RenderResolutionHelper.h"
    "Source/HaltonSequence.h"
    "Source/LightLists.h"
    "Source/LightDefs.h"
    "Source/SectorVisibility.h"
    "Source/TriangleInfoManager.h"
    "Source/LensFlares.h"
    "Source/DecalManager.h"
    "Source/EffectBase.h"
    "Source/EffectWipe.h"
    "Source/EffectSimple.h"
    "Source/EffectSimple_Instances.h"
    "Source/LightGrid.h"
    "Source/FSR2.h"
)

set(Sources
    "Source/RTGL1.cpp"
    "Source/RTGL1A.cpp"
    "Source/VulkanDevice.cpp"
    "Source/Buffer.cpp"
    "Source/Scene.cpp"
    "Source/PhysicalDevice.cpp"
    "Source/Queues.cpp"
    "Source/Swapchain.cpp"
    "Source/GlobalUniform.cpp"
    "Source/CommandBufferManager.cpp"
    "Source/ShaderManager.cpp"
    "Source/RayTracingPipeline.cpp"
    "Source/VertexCollector.cpp"
    "Source/ASManager.cpp"
    "Source/VertexCollectorFilter.cpp"
    "Source/ASBuilder.cpp"
    "Source/ScratchBuffer.cpp"
    "Source/Utils.cpp"
    "Source/PathTracer.cpp"
    "Source/Common.cpp"
    "Source/Matrix.cpp"
    "Source/Rasterizer.cpp"
    "Source/RasterizedDataCollector.cpp"
    "Source/Vma/vk_mem_alloc_imp.cpp"
    "Source/ImageLoader.cpp" 
    "Source/TextureManager.cpp" 
    "Source/MemoryAllocator.cpp" 
    "Source/SamplerManager.cpp" 
    "Source/TextureOverrides.cpp"
    "Source/TextureDescriptors.cpp" 
    "Source/TextureUploader.cpp"
    "Source/VertexCollectorFilterType.cpp"
    "Source/Generated/ShaderCommonCFramebuf.cpp" 
    "Source/Framebuffers.cpp"
    "Source/BlueNoise.cpp"
    "Source/ImageComposition.cpp"
    "Source/Tonemapping.cpp"
    "Source/LightManager.cpp"
    "Source/AutoBuffer.cpp"
    "Source/ASComponent.cpp"
    "Source/CubemapManager.cpp"
    "Source/CubemapUploader.cpp"
    "Source/GeomInfoManager.cpp"
    "Source/VertexPreprocessing.cpp"
    "Source/Denoiser.cpp"
    "Source/RasterizerPipelines.cpp"
    "Source/RenderCubemap.cpp"
    "Source/DepthCopying.cpp"
    "Source/RasterPass.cpp"
    "Source/SwapchainPass.cpp"
    "Source/UserFunction.cpp"    
    "Source/RgException.cpp"
    "Source/Bloom.cpp"
    "Source/SuperResolution.cpp"
    "Source/Sharpening.cpp"
    "Source/DLSS.cpp"
    "Source/HaltonSequence.cpp"
    "Source/LightLists.cpp"
    "Source/SectorVisibility.cpp"
    "Source/TriangleInfoManager.cpp"
    "Source/LensFlares.cpp"
    "Source/DecalManager.cpp"
    "Source/EffectBase.cpp"
    "Source/LightGrid.cpp"
    "Source/FSR2.cpp"
)



set(KTXSourceFolder Source/KTX/lib)

set(KTXSources
    "${KTXSourceFolder}/texture.c"
    "${KTXSourceFolder}/texture1.c"
    "${KTXSourceFolder}/texture2.c"
    "${KTXSourceFolder}/hashlist.c"
    "${KTXSourceFolder}/checkheader.c"
    "${KTXSourceFolder}/swap.c"
    "${KTXSourceFolder}/memstream.c"
    "${KTXSourceFolder}/filestream.c"
    "${KTXSourceFolder}/vkloader.c"
    "${KTXSourceFolder}/vk_funcs.c"
    "${KTXSourceFolder}/basisu/zstd/zstd.c"
    "${KTXSourceFolder}/dfdutils/createdfd.c"
    "${KTXSourceFolder}/dfdutils/interpretdfd.c"
    "${KTXSourceFolder}/dfdutils/queries.c"
    "${KTXSourceFolder}/dfdutils/vk2dfd.c"
)

option(RG_WITH_EXPORTS          "Compile library for export"                ON)

option(RG_WITH_SURFACE_WIN32    "Build with Win32 VkSurfaceKHR support"     OFF)
option(RG_WITH_SURFACE_METAL    "Build with Metal VkSurfaceKHR support"     OFF)
option(RG_WITH_SURFACE_WAYLAND  "Build with Wayland VkSurfaceKHR support"   OFF)
option(RG_WITH_SURFACE_XCB      "Build with Xcb VkSurfaceKHR support"       OFF)
option(RG_WITH_SURFACE_XLIB     "Build with Xlib VkSurfaceKHR support"      OFF)

option(RG_WITH_NVIDIA_DLSS      "Build RTGL1 with Nvidia DLSS"              OFF)

option(RG_WITH_EXAMPLES         "Add examples project"                      OFF)


# for KTX-Software
add_definitions(-DKHRONOS_STATIC -DLIBKTX)


# options to definitions
if (RG_WITH_EXPORTS)
    message(STATUS "RG_WITH_EXPORTS enabled. RTGL1 functions are exported.")
    add_definitions(-DRG_LIBRARY_EXPORTS)
endif()

if (RG_WITH_SURFACE_WIN32)
    message(STATUS "RG_WITH_SURFACE_WIN32 enabled")
    add_definitions(-DNOMINMAX)
    add_definitions(-DRG_USE_SURFACE_WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
endif()
if (RG_WITH_SURFACE_METAL)
    message(STATUS "RG_WITH_SURFACE_METAL enabled")
    add_definitions(-DRG_USE_SURFACE_METAL)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif()
if (RG_WITH_SURFACE_WAYLAND)
    message(STATUS "RG_WITH_SURFACE_WAYLAND enabled")
    add_definitions(-DRG_USE_SURFACE_WAYLAND)
    add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)
endif()
if (RG_WITH_SURFACE_XCB)
    message(STATUS "RG_WITH_SURFACE_XCB enabled")
    add_definitions(-DRG_USE_SURFACE_XCB)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
endif()
if (RG_WITH_SURFACE_XLIB)
    message(STATUS "RG_WITH_SURFACE_XLIB enabled")
    add_definitions(-DRG_USE_SURFACE_XLIB)
    add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
endif()


add_library(RayTracedGL1 SHARED  
    ${Sources}
    ${KTXSources}
    ${PublicHeaders}
)

# KTX
target_include_directories(RayTracedGL1 PRIVATE "Source/KTX/include")
target_include_directories(RayTracedGL1 PRIVATE "Source/KTX/include" "Source/KTX/other_include" "Source/KTX/lib/basisu/zstd")

# DLSS
if (RG_WITH_NVIDIA_DLSS)
    message(STATUS "RG_USE_NVIDIA_DLSS enabled")
    add_definitions(-DRG_USE_NVIDIA_DLSS)

    if (DEFINED ENV{DLSS_SDK_PATH})
        message(STATUS "Found DLSS SDK: $ENV{DLSS_SDK_PATH}")
        target_include_directories(RayTracedGL1 PRIVATE "$ENV{DLSS_SDK_PATH}/include")
        if (WIN32)
            add_definitions(-DNV_WINDOWS)
            string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE )
            if (BUILD_TYPE STREQUAL "DEBUG")
                target_link_libraries(RayTracedGL1 PRIVATE "$ENV{DLSS_SDK_PATH}/lib/Windows_x86_64/x86_64/nvsdk_ngx_d_dbg.lib")
            else()
                target_link_libraries(RayTracedGL1 PRIVATE "$ENV{DLSS_SDK_PATH}/lib/Windows_x86_64/x86_64/nvsdk_ngx_d.lib")
            endif()
        else()
            target_link_libraries(RayTracedGL1 PRIVATE "$ENV{DLSS_SDK_PATH}/lib/Linux_x86_64/libnvsdk_ngx.a")
            target_link_libraries(RayTracedGL1 PRIVATE ${CMAKE_DL_LIBS})
        endif()
    else()
        message(FATAL_ERROR 
            "Can't find DLSS_SDK_PATH environment variable. "
            "Please, provide the path to https://github.com/NVIDIA/DLSS, to configure the library")
    endif()
endif()

# Vulkan
target_link_libraries(RayTracedGL1 PUBLIC Vulkan)
target_include_directories(RayTracedGL1 PUBLIC "Include")

# FSR2
target_include_directories(RayTracedGL1 PRIVATE "Source/FSR2/include" )
if (WIN32)
    if (BUILD_TYPE STREQUAL "DEBUG")
        target_link_libraries(RayTracedGL1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/FSR2/win32/ffx_fsr2_api_x64d.lib" )
        target_link_libraries(RayTracedGL1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/FSR2/win32/ffx_fsr2_api_vk_x64d.lib" )
    else()
        target_link_libraries(RayTracedGL1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/FSR2/win32/ffx_fsr2_api_x64.lib" )
        target_link_libraries(RayTracedGL1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/FSR2/win32/ffx_fsr2_api_vk_x64.lib" )
    endif()
else()
    message(FATAL_ERROR "On non-WIN32, please, provide static libraries of FSR2 or disable FSR2")
endif()


if (RG_WITH_EXAMPLES)
    message(STATUS "RG_WITH_EXAMPLES enabled")
    set(RTGL1_EXAMPLES_STANDALONE OFF CACHE BOOL "" FORCE)
    set(RTGL1_SDK_PATH "${CMAKE_SOURCE_DIR}")
    add_subdirectory(Tests)
endif()
