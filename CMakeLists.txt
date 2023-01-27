cmake_minimum_required(VERSION 3.20)
project(RayTracedGL1 CXX C)

set(CMAKE_CXX_STANDARD 23)

# Vulkan
message(STATUS "Adding Vulkan. VulkanSDK: $ENV{VULKAN_SDK}")
find_package(Vulkan REQUIRED)
add_library(Vulkan INTERFACE)
target_include_directories(Vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Vulkan INTERFACE ${Vulkan_LIBRARIES})

set(PublicHeaders 
    Include/RTGL1/RTGL1.h
)

set(Sources
    "Source/RTGL1.cpp"
    "Source/RTGL1A.cpp"
    "Source/VulkanDevice.cpp"
    "Source/VulkanDevice_Init.cpp"
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
    "Source/RgException.cpp"
    "Source/Bloom.cpp"
    "Source/Sharpening.cpp"
    "Source/DLSS.cpp"
    "Source/HaltonSequence.cpp"
    "Source/LensFlares.cpp"
    "Source/DecalManager.cpp"
    "Source/EffectBase.cpp"
    "Source/LightGrid.cpp"
    "Source/FSR2.cpp"
    "Source/Stb/stb_image.cpp"
    "Source/Stb/stb_image_write.cpp"
    "Source/ImageLoaderDev.cpp"
    "Source/PortalList.cpp"
    "Source/RestirBuffers.cpp"
    "Source/Volumetric.cpp"
    "Source/DebugWindows.cpp"
    "Source/ScratchImmediate.cpp"
    "Source/GltfExporter.cpp"
    "Source/GltfImporter.cpp"
    "Source/FolderObserver.cpp"
    "Source/TextureExporter.cpp"
    "Source/TextureMeta.cpp"
    "Source/VulkanDevice_Dev.cpp"
    "Source/JsonParser.cpp"
    "Source/SceneMeta.cpp"
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
option(RG_WITH_IMGUI            "Build RTGL1 with ImGui debug windows"      ON)

option(RG_WITH_EXAMPLES         "Build with examples executable"            ON)


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


# Imgui
if (RG_WITH_IMGUI)
    message(STATUS "RG_WITH_IMGUI enabled")
    add_definitions(-DRG_USE_IMGUI)
    set(ImGuiSourceFolder Source/imgui)
    add_library(ImGui STATIC)
    target_sources(ImGui PRIVATE 
        ${ImGuiSourceFolder}/imgui_demo.cpp
        ${ImGuiSourceFolder}/imgui_draw.cpp
        ${ImGuiSourceFolder}/imgui_tables.cpp
        ${ImGuiSourceFolder}/imgui_widgets.cpp
        ${ImGuiSourceFolder}/imgui.cpp
        ${ImGuiSourceFolder}/backends/imgui_impl_vulkan.cpp
        ${ImGuiSourceFolder}/backends/imgui_impl_glfw.cpp
    )
    target_include_directories(ImGui PUBLIC ${ImGuiSourceFolder} ${ImGuiSourceFolder}/backends)
    # Use Vulkan and GLFW as ImGui backend
    target_link_libraries(ImGui PUBLIC Vulkan)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(Source/glfw)
    target_link_libraries(ImGui PRIVATE glfw)
    target_include_directories(ImGui PRIVATE glfw)
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
    message(STATUS "RG_WITH_NVIDIA_DLSS enabled")
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

# Debug windows - ImGui
if (RG_WITH_IMGUI)
    target_link_libraries(RayTracedGL1 PRIVATE ImGui)
    target_include_directories(RayTracedGL1 PRIVATE ImGui)
    target_link_libraries(RayTracedGL1 PRIVATE glfw)
    target_include_directories(RayTracedGL1 PRIVATE glfw)
endif()

# Json parser - glaze
add_subdirectory(Source/glaze)
target_link_libraries(RayTracedGL1 PRIVATE glaze::glaze)

if (RG_WITH_EXAMPLES)
    message(STATUS "RG_WITH_EXAMPLES enabled")
    add_definitions(-DASSET_DIRECTORY="../")
    add_executable(RtglExample Tests/RtglTest.cpp)
    set_property(TARGET RtglExample PROPERTY CXX_STANDARD 20)
    target_link_libraries(RtglExample RayTracedGL1)
    target_link_libraries(RtglExample glfw)
    target_include_directories(RtglExample PUBLIC Tests/Libs/glm)
endif()

# VS hot-reload - disabled because of glaze
if (false)
if (MSVC AND WIN32 AND NOT MSVC_VERSION VERSION_LESS 142)
    target_link_options(RayTracedGL1 PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(RayTracedGL1 PRIVATE $<$<CONFIG:Debug>:/ZI>)

    target_link_options(RtglExample PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(RtglExample PRIVATE $<$<CONFIG:Debug>:/ZI>)
endif()
endif()