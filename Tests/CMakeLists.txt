cmake_minimum_required(VERSION 3.15)
project(RtglTest CXX)


message(STATUS "Adding standalone example") 
set(RTGL1_SDK_PATH "${CMAKE_SOURCE_DIR}/..")


# add RTGL1 library using .dll / .so
add_library(RayTracedGL1 SHARED IMPORTED)
if (WIN32)
    set_target_properties(RayTracedGL1 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${RTGL1_SDK_PATH}/Include"
        IMPORTED_LOCATION       "${RTGL1_SDK_PATH}/Build/x64-Release/RayTracedGL1.dll"
        IMPORTED_LOCATION_DEBUG "${RTGL1_SDK_PATH}/Build/x64-Debug/RayTracedGL1.dll"
        IMPORTED_IMPLIB         "${RTGL1_SDK_PATH}/Build/x64-Release/RayTracedGL1.lib"
        IMPORTED_IMPLIB_DEBUG   "${RTGL1_SDK_PATH}/Build/x64-Debug/RayTracedGL1.lib"
    )
else()
    set_target_properties(RayTracedGL1 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${RTGL1_SDK_PATH}/Include"
        IMPORTED_LOCATION       "${RTGL1_SDK_PATH}/Build/RelWithDebInfo/libRayTracedGL1.so"
        IMPORTED_LOCATION_DEBUG "${RTGL1_SDK_PATH}/Build/Debug/libRayTracedGL1.so"
        IMPORTED_IMPLIB         "${RTGL1_SDK_PATH}/Build/RelWithDebInfo/libRayTracedGL1.so"
        IMPORTED_IMPLIB_DEBUG   "${RTGL1_SDK_PATH}/Build/Debug/libRayTracedGL1.so"
    )
endif()


# find glfw library
 set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Libs/glfw)


add_executable(RtglTest RtglTest.cpp)
set_property(TARGET RtglTest PROPERTY CXX_STANDARD 20)

target_link_libraries(RtglTest RayTracedGL1)
target_link_libraries(RtglTest glfw)
target_include_directories(RtglTest PUBLIC Libs/glm)


# post-build step: copy dll-s to exe directory
get_target_property(RTGL1_DLL_PATH RayTracedGL1 IMPORTED_LOCATION)
get_target_property(RTGL1_DLL_PATH_D RayTracedGL1 IMPORTED_LOCATION_DEBUG)
add_custom_command(TARGET RtglTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<$<CONFIG:Debug>:${RTGL1_DLL_PATH_D}>"
    "$<$<CONFIG:MinSizeRel>:${RTGL1_DLL_PATH}>"
    "$<$<CONFIG:RelWithDebInfo>:${RTGL1_DLL_PATH}>"
    "$<$<CONFIG:Release>:${RTGL1_DLL_PATH}>"
    $<TARGET_FILE_DIR:RtglTest>/${CMAKE_SHARED_LIBRARY_PREFIX}RayTracedGL1${CMAKE_SHARED_LIBRARY_SUFFIX}
)