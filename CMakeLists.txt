cmake_minimum_required(VERSION 3.15)
project(RayTracedGL1 CXX C)

set(CMAKE_CXX_STANDARD 11)

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
    "Source/Generated/BlueNoiseFileNames.h"
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
)



set(KTXSourceFolder Source/KTX/lib)

set(KTXSources
    ${KTXSourceFolder}"/texture.c"
    ${KTXSourceFolder}"/texture1.c"
    ${KTXSourceFolder}"/texture2.c"
    ${KTXSourceFolder}"/hashlist.c"
    ${KTXSourceFolder}"/checkheader.c"
    ${KTXSourceFolder}"/swap.c"
    ${KTXSourceFolder}"/memstream.c"
    ${KTXSourceFolder}"/filestream.c"
    ${KTXSourceFolder}"/vkloader.c"
    ${KTXSourceFolder}"/vk_funcs.c"
    ${KTXSourceFolder}"/basisu/zstd/zstd.c"
    ${KTXSourceFolder}"/dfdutils/createdfd.c"
    ${KTXSourceFolder}"/dfdutils/interpretdfd.c"
    ${KTXSourceFolder}"/dfdutils/queries.c"
    ${KTXSourceFolder}"/dfdutils/vk2dfd.c"
)

add_definitions(-DKHRONOS_STATIC -DLIBKTX)

add_library(RayTracedGL1 STATIC  
    ${Sources}
    ${KTXSources}
    ${PublicHeaders}
)
target_include_directories(RayTracedGL1 PRIVATE "Source/KTX/include")
target_include_directories(RayTracedGL1 PRIVATE "Source/KTX/include" "Source/KTX/other_include" "Source/KTX/lib/basisu/zstd")

target_link_libraries(RayTracedGL1 PUBLIC Vulkan)
target_include_directories(RayTracedGL1 PUBLIC "Include")



# optional tests
option(RAYTRACEDGL1_TESTS "Add tests and examples for library" ON)
if (RAYTRACEDGL1_TESTS)
    message(STATUS "Adding tests.")
    add_subdirectory(Tests)
endif()