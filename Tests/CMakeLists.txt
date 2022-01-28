cmake_minimum_required(VERSION 3.15)
project(RtglTest CXX)

message(STATUS "Adding tests.")
    

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Libs/glfw)


set(RTGL1_SDK_PATH "${CMAKE_SOURCE_DIR}/..")

# link to RTGL1
add_library(RayTracedGL1 SHARED IMPORTED)
set_target_properties(RayTracedGL1 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${RTGL1_SDK_PATH}/Include"
    IMPORTED_LOCATION       "${RTGL1_SDK_PATH}/Build/x64-Release/RayTracedGL1.dll"
    IMPORTED_LOCATION_DEBUG "${RTGL1_SDK_PATH}/Build/x64-Debug/RayTracedGL1.dll"
    IMPORTED_IMPLIB         "${RTGL1_SDK_PATH}/Build/x64-Release/RayTracedGL1.lib"
    IMPORTED_IMPLIB_DEBUG   "${RTGL1_SDK_PATH}/Build/x64-Debug/RayTracedGL1.lib"
)

add_executable(RtglTest 
    Libs/tinyobjloader/tiny_obj_loader.h 
    RtglTest.cpp)

target_link_libraries(RtglTest RayTracedGL1)
target_link_libraries(RtglTest glfw)
target_include_directories(RtglTest PUBLIC )
target_include_directories(RtglTest PUBLIC Libs/glm)
