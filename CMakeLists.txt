cmake_minimum_required(VERSION 3.15)
project(RayTracedGL1 CXX)

set(CMAKE_CXX_STANDARD 11)

# Vulkan
message(STATUS "Adding Vulkan. VulkanSDK: $ENV{VULKAN_SDK}")
find_package(Vulkan REQUIRED)
add_library(Vulkan INTERFACE)
target_include_directories(Vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Vulkan INTERFACE ${Vulkan_LIBRARIES})

set(PublicHeaders 
    Include/RTGL1/RTGL1.h
    Include/RTGL1/RTGL1A.h
)

set(Sources
    Source/RTGL1.cpp
    Source/RTGL1A.cpp "Source/VulkanDevice.cpp" "Source/Buffer.cpp" "Source/Scene.cpp" "Source/PhysicalDevice.cpp" "Source/Queues.cpp" "Source/Swapchain.cpp" "Source/GlobalUniform.cpp" "Source/CommandBufferManager.cpp" "Source/ShaderManager.cpp" "Source/RayTracingPipeline.cpp" "Source/VertexCollector.cpp" "Source/VertexBufferManager.cpp" "Source/VertexCollectorFiltered.cpp" "Source/ASBuilder.cpp" "Source/ScratchBuffer.cpp")

add_library(RayTracedGL1 STATIC  
    ${Sources}
    ${PublicHeaders}
)

target_include_directories(RayTracedGL1 PUBLIC Include)
target_link_libraries(RayTracedGL1 PUBLIC Vulkan)

# optional tests
option(RAYTRACEDGL1_TESTS "Add tests and examples for library" ON)
if (RAYTRACEDGL1_TESTS)
    message(STATUS "Adding tests.")
    add_subdirectory(Tests)
endif()