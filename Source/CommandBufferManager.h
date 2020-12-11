#pragma once

#include <map>
#include <vector>
#include "Common.h"
#include "Queues.h"

class CommandBufferManager
{
public:
    CommandBufferManager(VkDevice device, std::shared_ptr<Queues> queues);
    ~CommandBufferManager();

    void PrepareForFrame(uint32_t frameIndex);

    // Start graphics command buffer for current frame index
    VkCommandBuffer StartGraphicsCmd();
    // Start compute command buffer for current frame index
    VkCommandBuffer StartComputeCmd();
    // Start transfer command buffer for current frame index
    VkCommandBuffer StartTransferCmd();

    void Submit(VkCommandBuffer cmd, VkFence fence = VK_NULL_HANDLE);
    void Submit(VkCommandBuffer cmd, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages, VkSemaphore signalSemaphore, VkFence fence);

    void WaitForFence(VkFence fence);

    void WaitGraphicsIdle();
    void WaitComputeIdle();
    void WaitTransferIdle();
    void WaitDeviceIdle();

private:
    VkCommandBuffer StartCmd(uint32_t frameIndex, VkCommandPool cmdPool, VkQueue queue);

private:
    VkDevice device;

    uint32_t currentFrameIndex;

    VkCommandPool graphicsPools[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool computePools[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool transferPools[MAX_FRAMES_IN_FLIGHT];

    std::weak_ptr<Queues> queues;
    std::map<VkCommandBuffer, VkQueue> cmdQueues[MAX_FRAMES_IN_FLIGHT];
};
