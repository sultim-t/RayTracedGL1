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

#pragma once

#include <cassert>
#include <cstring>
#include <functional>
#include <memory>

#include <vulkan/vulkan.h>

#include "RTGL1/RTGL1.h"

#include "DebugPrint.h"

namespace RTGL1
{

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

#pragma region extension functions

// extension functions' lists
#define VK_INSTANCE_DEBUG_UTILS_FUNCTION_LIST               \
    VK_EXTENSION_FUNCTION( vkCreateDebugUtilsMessengerEXT ) \
    VK_EXTENSION_FUNCTION( vkDestroyDebugUtilsMessengerEXT )

#define VK_DEVICE_FUNCTION_LIST                                         \
    VK_EXTENSION_FUNCTION( vkCmdPipelineBarrier2KHR )                   \
    VK_EXTENSION_FUNCTION( vkCreateAccelerationStructureKHR )           \
    VK_EXTENSION_FUNCTION( vkDestroyAccelerationStructureKHR )          \
    VK_EXTENSION_FUNCTION( vkGetRayTracingShaderGroupHandlesKHR )       \
    VK_EXTENSION_FUNCTION( vkCreateRayTracingPipelinesKHR )             \
    VK_EXTENSION_FUNCTION( vkGetAccelerationStructureDeviceAddressKHR ) \
    VK_EXTENSION_FUNCTION( vkGetAccelerationStructureBuildSizesKHR )    \
    VK_EXTENSION_FUNCTION( vkCmdBuildAccelerationStructuresKHR )        \
    VK_EXTENSION_FUNCTION( vkCmdTraceRaysKHR )

#define VK_DEVICE_DEBUG_UTILS_FUNCTION_LIST               \
    VK_EXTENSION_FUNCTION( vkSetDebugUtilsObjectNameEXT ) \
    VK_EXTENSION_FUNCTION( vkCmdBeginDebugUtilsLabelEXT ) \
    VK_EXTENSION_FUNCTION( vkCmdEndDebugUtilsLabelEXT )


// extension functions' declarations
#define VK_EXTENSION_FUNCTION( fname ) extern PFN_##fname s##fname;
VK_INSTANCE_DEBUG_UTILS_FUNCTION_LIST
VK_DEVICE_FUNCTION_LIST
VK_DEVICE_DEBUG_UTILS_FUNCTION_LIST
#undef VK_EXTENSION_FUNCTION

void InitInstanceExtensionFunctions_DebugUtils( VkInstance instance );
void InitDeviceExtensionFunctions( VkDevice device );
void InitDeviceExtensionFunctions_DebugUtils( VkDevice device );

#pragma endregion


inline void VK_CHECKERROR( const VkResult r )
{
    assert( r == VK_SUCCESS );
}


#define SET_DEBUG_NAME( device, obj, type, pName ) \
    RTGL1::AddDebugName( ( device ), reinterpret_cast< uint64_t >( obj ), ( type ), ( pName ) )


// If name is null, debug name won't be set
void AddDebugName( VkDevice device, uint64_t obj, VkObjectType type, const char* pName );
void BeginCmdLabel( VkCommandBuffer cmd, const char* pName, const float pColor[ 4 ] = nullptr );
void EndCmdLabel( VkCommandBuffer cmd );

}
