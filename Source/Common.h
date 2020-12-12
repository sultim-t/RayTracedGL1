#pragma once

#include <cassert>
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#define MAX_FRAMES_IN_FLIGHT 2

#pragma region extension functions definition
#define VK_INSTANCE_FUNCTION_LIST \
	VK_EXTENSION_FUNCTION(CmdBeginDebugUtilsLabelEXT) \
	VK_EXTENSION_FUNCTION(CmdEndDebugUtilsLabelEXT) \
	VK_EXTENSION_FUNCTION(CreateDebugUtilsMessengerEXT) \
	VK_EXTENSION_FUNCTION(DestroyDebugUtilsMessengerEXT)


#define VK_DEVICE_FUNCTION_LIST \
	VK_EXTENSION_FUNCTION(DebugMarkerSetObjectNameEXT) \
	VK_EXTENSION_FUNCTION(BindAccelerationStructureMemoryKHR) \
	VK_EXTENSION_FUNCTION(CreateAccelerationStructureKHR) \
	VK_EXTENSION_FUNCTION(DestroyAccelerationStructureKHR) \
	VK_EXTENSION_FUNCTION(GetRayTracingShaderGroupHandlesKHR) \
	VK_EXTENSION_FUNCTION(CreateRayTracingPipelinesKHR) \
	VK_EXTENSION_FUNCTION(GetAccelerationStructureMemoryRequirementsKHR) \
	VK_EXTENSION_FUNCTION(GetAccelerationStructureDeviceAddressKHR) \
	VK_EXTENSION_FUNCTION(CmdBuildAccelerationStructureKHR) \
	VK_EXTENSION_FUNCTION(CmdTraceRaysKHR) \
	VK_EXTENSION_FUNCTION(GetBufferDeviceAddressKHR) 


#define VK_EXTENSION_FUNCTION(fname) static PFN_vk##fname vks##fname;
VK_INSTANCE_FUNCTION_LIST
VK_DEVICE_FUNCTION_LIST
#undef VK_EXTENSION_FUNCTION


void InitInstanceExtensionFunctions(VkInstance instance)
{
#define VK_EXTENSION_FUNCTION(fname) \
		vks##fname = (PFN_vk##fname)vkGetInstanceProcAddr(instance, "vk"#fname); \
		assert(vks##fname != nullptr);

    VK_INSTANCE_FUNCTION_LIST
    #undef VK_EXTENSION_FUNCTION
}

void InitDeviceExtensionFunctions(VkDevice device)
{
#define VK_EXTENSION_FUNCTION(fname) \
		vks##fname = (PFN_vk##fname)vkGetDeviceProcAddr(device, "vk"#fname); \
		assert(vks##fname != nullptr);

    VK_DEVICE_FUNCTION_LIST
    #undef VK_EXTENSION_FUNCTION
}


#undef VK_INSTANCE_FUNCTION_LIST
#undef VK_DEVICE_FUNCTION_LIST
#pragma endregion


#define VK_CHECKERROR(x) assert(x == VK_SUCCESS)

#ifndef _NDEBUG
#define SET_DEBUG_NAME(device, obj, type, name) AddDebugName(device, obj, type, name)
#else
#define SET_DEBUG_NAME(device, obj, type, name) do{}while(0)
#endif

void AddDebugName(VkDevice device, uint64_t obj, VkDebugReportObjectTypeEXT type, const char *name)
{
    VkDebugMarkerObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    nameInfo.object = obj;
    nameInfo.objectType = type;
    nameInfo.pObjectName = name;

    VkResult r = vksDebugMarkerSetObjectNameEXT(device, &nameInfo);
    VK_CHECKERROR(r);
}

// TODO: remove VBProperties from here 
struct VBProperties
{
    bool vertexArrayOfStructs;
    uint32_t positionStride;
    uint32_t normalStride;
    uint32_t texCoordStride;
    uint32_t colorStride;
};