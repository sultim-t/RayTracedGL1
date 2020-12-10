#pragma once
#include "Common.h"
#include "Buffer.h"

class VertexBuffer
{
    VertexBuffer(VkDevice device, const PhysicalDevice &physDevice);
    ~VertexBuffer();

private:
    void CreateDescriptors();

private:
    VkDevice device;

    Buffer staticVertsBuffer;
    Buffer staticVertsStaging;
    Buffer dynamicVertsBuffer[MAX_FRAMES_IN_FLIGHT];
    Buffer dynamicVertsStaging[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool descPool;
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];
};
