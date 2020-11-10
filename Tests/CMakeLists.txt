cmake_minimum_required(VERSION 3.15)

add_executable(BasicTest Basic/BasicTest.cpp)
target_link_libraries(BasicTest RayTracedGL1)

project(BasicTestC C)
add_executable(BasicTestC Basic/BasicTestC.c)
target_link_libraries(BasicTestC RayTracedGL1)

add_executable(RaytracingTest VkUtils.h tiny_obj_loader.h Raytracing.cpp)
target_link_libraries(RaytracingTest RayTracedGL1)
target_link_libraries(RaytracingTest glfw)
target_include_directories(RaytracingTest PUBLIC glm)
