#include "Common.h"

// extension functions' definitions
#define VK_EXTENSION_FUNCTION(fname) PFN_##fname s##fname;
    VK_INSTANCE_FUNCTION_LIST
    VK_DEVICE_FUNCTION_LIST
#undef VK_EXTENSION_FUNCTION


void InitInstanceExtensionFunctions(VkInstance instance)
{
    #define VK_EXTENSION_FUNCTION(fname) \
		s##fname = (PFN_##fname)vkGetInstanceProcAddr(instance, #fname); \
		assert(s##fname != nullptr);

    VK_INSTANCE_FUNCTION_LIST
    #undef VK_EXTENSION_FUNCTION
}

void InitDeviceExtensionFunctions(VkDevice device)
{
    #define VK_EXTENSION_FUNCTION(fname) \
		s##fname = (PFN_##fname)vkGetDeviceProcAddr(device, #fname); \
		assert(s##fname != nullptr);

    VK_DEVICE_FUNCTION_LIST
    #undef VK_EXTENSION_FUNCTION
}

void AddDebugName(VkDevice device, uint64_t obj, VkDebugReportObjectTypeEXT type, const char *name)
{
    VkDebugMarkerObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    nameInfo.object = obj;
    nameInfo.objectType = type;
    nameInfo.pObjectName = name;

    VkResult r = svkDebugMarkerSetObjectNameEXT(device, &nameInfo);
    VK_CHECKERROR(r);
}
