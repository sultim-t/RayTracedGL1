#include "VertexBufferStatic.h"
#include "Generated/ShaderCommonC.h"

VertexBufferStatic::VertexBufferStatic(VkDevice device, const PhysicalDevice &physDevice)
{


}

VertexBufferStatic::~VertexBufferStatic()
{
}

void VertexBufferStatic::BeginCollecting()
{

    CreateBuffer(mainVk.device, mainVk.physicalDeviceProperties,
                 SCRATCH_BUFFER_SIZE,
                 VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 rtglData.scratchBuffer);

}

void VertexBufferStatic::EndCollecting()
{

}

void VertexBufferStatic::Submit()
{
}

void VertexBufferStatic::Reset()
{

}
