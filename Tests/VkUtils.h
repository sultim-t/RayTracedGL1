#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <algorithm>

#pragma region extension functions definition
#define VK_INSTANCE_FUNCTION_LIST \
	VK_EXTENSION_FUNCTION(CmdBeginDebugUtilsLabelEXT) \
	VK_EXTENSION_FUNCTION(CmdEndDebugUtilsLabelEXT) \
	VK_EXTENSION_FUNCTION(CreateDebugUtilsMessengerEXT)


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



class FrameCmdBuffers
{
private:
	VkDevice device;
	VkCommandPool cmdPool;
	VkQueue queue;
	uint32_t usedCount;
	std::vector<VkCommandBuffer> cmdBuffers;

public:
	FrameCmdBuffers() :
		cmdPool(VK_NULL_HANDLE), queue(VK_NULL_HANDLE), usedCount(0)
	{
		assert(cmdBuffers.size() == 0);
	}

	FrameCmdBuffers(VkDevice vdevice, VkCommandPool pool, VkQueue submitQueue) :
		device(vdevice), cmdPool(pool), queue(submitQueue), usedCount(0)
	{
		assert(cmdBuffers.size() == 0);
	}

	void Reset()
	{
		usedCount = 0;
	}

	void WaitIdle()
	{
		vkQueueWaitIdle(queue);
		Reset();
	}

	VkCommandBuffer BeginCmd()
	{
		VkResult r;

		if (usedCount >= cmdBuffers.size())
		{
			uint32_t newSize = std::max<uint32_t>(8, cmdBuffers.size() * 2);
			uint32_t toAlloc = newSize - cmdBuffers.size();

			cmdBuffers.resize(newSize);

			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = cmdPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = toAlloc;

			r = vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffers[newSize - toAlloc]);
			VK_CHECKERROR(r);
		}

		VkCommandBuffer cmd = cmdBuffers[usedCount];

		r = vkResetCommandBuffer(cmd, 0);
		VK_CHECKERROR(r);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		r = vkBeginCommandBuffer(cmd, &beginInfo);
		VK_CHECKERROR(r);

		usedCount++;

		return cmd;
	}

	void Submit(VkCommandBuffer cmd, VkFence fence = VK_NULL_HANDLE)
	{
		VkResult r = vkEndCommandBuffer(cmd);
		VK_CHECKERROR(r);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		r = vkQueueSubmit(queue, 1, &submitInfo, fence);
		VK_CHECKERROR(r);
	}

	void Submit(VkCommandBuffer cmd, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages, VkSemaphore signalSemaphore, VkFence fence)
	{
		VkResult r = vkEndCommandBuffer(cmd);
		VK_CHECKERROR(r);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphore;
		submitInfo.pWaitDstStageMask = &waitStages;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphore;

		r = vkQueueSubmit(queue, 1, &submitInfo, fence);
		VK_CHECKERROR(r);
	}
};


struct FrameSemaphores
{
	VkSemaphore imageAvailable;
	VkSemaphore renderFinished;
	VkSemaphore transferFinished;
	VkSemaphore traceFinished;
	bool traceSignaled;
};


class Buffer
{
public:
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceAddress address;
	size_t size;
	bool isMapped;

public:
	void *Map(VkDevice device)
	{
		assert(!isMapped);
		assert(memory != VK_NULL_HANDLE && size > 0);
		
		isMapped = true;
		void *mapped;
		
		VkResult r = vkMapMemory(device, memory, 0, size, 0, &mapped);
		VK_CHECKERROR(r);

		return mapped;
	}

	void Unmap(VkDevice device)
	{
		assert(isMapped);
		isMapped = false;
		vkUnmapMemory(device, memory);
	}
};


/*class ScratchBuffer
{
	VkDevice device;
	VkBuffer buffer;
	VkDeviceMemory memory;
	uint64_t deviceAddress;

public:
	ScratchBuffer():
		device(VK_NULL_HANDLE), buffer(VK_NULL_HANDLE), 
		memory(VK_NULL_HANDLE), deviceAddress(0) {}

	ScratchBuffer(VkDevice vdevice) : 
		device(vdevice), buffer(VK_NULL_HANDLE),
		memory(VK_NULL_HANDLE), deviceAddress(0) {}

	void Create(VkAccelerationStructureKHR as, const VkPhysicalDeviceMemoryProperties &physMemProp)
	{
		VkResult r;
		assert(as != VK_NULL_HANDLE);
		assert(device != VK_NULL_HANDLE);
		assert(buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE && deviceAddress == 0);

		VkAccelerationStructureMemoryRequirementsInfoKHR memReqInfo = {};
		memReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
		memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
		memReqInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
		memReqInfo.accelerationStructure = as;

		VkMemoryRequirements2 memReq2 = {};
		memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
		vksGetAccelerationStructureMemoryRequirementsKHR(device, &memReqInfo, &memReq2);

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = memReq2.memoryRequirements.size;
		bufferInfo.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		r = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
		VK_CHECKERROR(r);

		VkMemoryRequirements befferMemReq = {};
		vkGetBufferMemoryRequirements(device, buffer, &befferMemReq);

		VkMemoryAllocateFlagsInfo allocFlagInfo = {};
		allocFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		allocFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = &allocFlagInfo;
		allocInfo.allocationSize = befferMemReq.size;
		allocInfo.memoryTypeIndex = GetMemoryTypeIndex(physMemProp, befferMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		r = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
		VK_CHECKERROR(r);

		r = vkBindBufferMemory(device, buffer, memory, 0);
		VK_CHECKERROR(r);
	}

	void Destroy()
	{
		assert(device != VK_NULL_HANDLE);
		assert(buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && deviceAddress != 0);
	}
};*/


VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData)
{
	const char *msg;

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
	{
		msg = "Vulkan::VERBOSE::[%d][%s]\n%s\n\n";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		msg = "Vulkan::INFO::[%d][%s]\n%s\n\n";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		msg = "Vulkan::WARNING::[%d][%s]\n%s\n\n";
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		msg = "Vulkan::ERROR::[%d][%s]\n%s\n\n";
	}
	else
	{
		msg = "Vulkan::[%d][%s]\n%s\n\n";
	}

	printf(msg, pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);

	return VK_FALSE;
}

uint32_t GetQueueFamilyIndex(std::vector<VkQueueFamilyProperties> queueFamilyProperties, VkQueueFlagBits queueFlags) 
{
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}

	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
	{
		if (queueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
		}
	}

	assert(0);
}

uint32_t GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties &physMemProp, uint32_t memoryTypeBits, VkFlags requirementsMask)
{
	// for each memory type available for this device
	for (uint32_t i = 0; i < physMemProp.memoryTypeCount; i++)
	{
		// if type is available
		if ((memoryTypeBits & 1u) == 1)
		{
			if ((physMemProp.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
			{
				return i;
			}
		}

		memoryTypeBits >>= 1u;
	}

	assert(0);
	return 0;
}

uint64_t GetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR addrInfo = {};
	addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = buffer;

	return vksGetBufferDeviceAddressKHR(device, &addrInfo);
}

void CreateBuffer(VkDevice device, const VkPhysicalDeviceMemoryProperties &physMemProp, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer &buffer)
{
	VkResult r;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	r = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.buffer);
	VK_CHECKERROR(r);

	VkMemoryRequirements memReq = {};
	vkGetBufferMemoryRequirements(device, buffer.buffer, &memReq);

	VkMemoryAllocateFlagsInfo allocFlagInfo = {};
	allocFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &allocFlagInfo;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = GetMemoryTypeIndex(physMemProp, memReq.memoryTypeBits, properties);

	r = vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory);
	VK_CHECKERROR(r);

	r = vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
	VK_CHECKERROR(r);

	buffer.size = size;
	buffer.address = GetBufferDeviceAddress(device, buffer.buffer);
}

void DestroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory bufferMemory)
{
	if (bufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, bufferMemory, nullptr);
	}

	if (buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(device, buffer, nullptr);
	}
}

void CopyToDeviceMemory(VkDevice device, VkDeviceMemory deviceMemory, const void *data, VkDeviceSize size)
{
	void *mapped;
	vkMapMemory(device, deviceMemory, 0, size, 0, &mapped);
	memcpy(mapped, data, (size_t) size);
	vkUnmapMemory(device, deviceMemory);
}

void BarrierImage(VkCommandBuffer cmd, VkImage image,
				  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
				  VkImageLayout oldLayout, VkImageLayout newLayout,
				  const VkImageSubresourceRange &subresourceRange)
{
	VkImageMemoryBarrier imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.image = image;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.srcAccessMask = srcAccessMask;
	imageBarrier.dstAccessMask = dstAccessMask;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(cmd,
						 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
						 0, nullptr,
						 0, nullptr,
						 1, &imageBarrier);
}

void BarrierImage(VkCommandBuffer cmd, VkImage image,
				  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
				  VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	BarrierImage(cmd, image, srcAccessMask, dstAccessMask, oldLayout, newLayout, subresourceRange);
}

VkDeviceMemory AllocDeviceMemory(VkDevice device, const VkPhysicalDeviceMemoryProperties &memProp, const VkMemoryRequirements &memReqs)
{
	VkDeviceMemory memory;

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = GetMemoryTypeIndex(memProp, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VkResult r = vkAllocateMemory(device, &memAllocInfo, nullptr, &memory);
	VK_CHECKERROR(r);

	return memory;
}

VkDeviceMemory AllocDeviceMemory(VkDevice device, const VkPhysicalDeviceMemoryProperties &memProp, const VkMemoryRequirements2 &memReqs2)
{
	return AllocDeviceMemory(device, memProp, memReqs2.memoryRequirements);
}

void AllocASMemory(VkDevice device, const VkPhysicalDeviceMemoryProperties &memProp, VkAccelerationStructureKHR as, VkDeviceMemory *outMemory)
{
	VkAccelerationStructureMemoryRequirementsInfoKHR memReqInfo = {};
	memReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
	memReqInfo.accelerationStructure = as;
	memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
	memReqInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

	VkMemoryRequirements2 memReq2 = {};
	memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	vksGetAccelerationStructureMemoryRequirementsKHR(device, &memReqInfo, &memReq2);

	*outMemory = AllocDeviceMemory(device, memProp, memReq2);
}

void BindASMemory(VkDevice device, VkAccelerationStructureKHR as, VkDeviceMemory memory)
{
	VkBindAccelerationStructureMemoryInfoKHR bindInfo = {};
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
	bindInfo.accelerationStructure = as;
	bindInfo.memory = memory;

	VkResult r = vksBindAccelerationStructureMemoryKHR(device, 1, &bindInfo);
	VK_CHECKERROR(r);
}

uint64_t GetASDeviceAddress(VkDevice device, VkAccelerationStructureKHR as)
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = as;

	return vksGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}