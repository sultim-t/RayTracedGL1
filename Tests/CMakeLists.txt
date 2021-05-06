cmake_minimum_required(VERSION 3.15)

message(STATUS "Adding tests.")
    

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Libs/glfw)


add_executable(RtglTest 
    Libs/tinyobjloader/tiny_obj_loader.h 
    RtglTest.cpp)

target_link_libraries(RtglTest RayTracedGL1)
target_link_libraries(RtglTest glfw)
target_include_directories(RtglTest PUBLIC Libs/glm)
