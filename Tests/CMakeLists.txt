cmake_minimum_required(VERSION 3.15)
    
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Libs/glfw)

add_executable(BasicTest Basic/BasicTest.cpp)
target_link_libraries(BasicTest RayTracedGL1)

project(BasicTestC C)
add_executable(BasicTestC Basic/BasicTestC.c)
target_link_libraries(BasicTestC RayTracedGL1)

add_executable(RaytracingTest 
    VkUtils.h 
    Libs/tinyobjloader/tiny_obj_loader.h 
    Raytracing.cpp)
target_link_libraries(RaytracingTest RayTracedGL1)
target_link_libraries(RaytracingTest glfw)
target_include_directories(RaytracingTest PUBLIC Libs/glm)

add_executable(RtglTest 
    Libs/tinyobjloader/tiny_obj_loader.h 
    RtglTest.cpp)
target_link_libraries(RtglTest RayTracedGL1)
target_link_libraries(RtglTest glfw)
target_include_directories(RtglTest PUBLIC Libs/glm)
