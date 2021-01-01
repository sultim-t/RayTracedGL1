#pragma once

#include <map>
#include <vector>

#include "Common.h"
#include "Queues.h"

class CommandBufferManager
{
public:
    explicit CommandBufferManager(VkDevice device, std::shared_ptr<Queues> queues);
    ~CommandBufferManager();

    CommandBufferManager(const CommandBufferManager& other) = delete;
    CommandBufferManager(CommandBufferManager&& other) noexcept = delete;
    CommandBufferManager& operator=(const CommandBufferManager& other) = delete;
    CommandBufferManager& operator=(CommandBufferManager&& other) noexcept = delete;

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
    struct AllocatedCmds
    {
        std::vector<VkCommandBuffer> cmds = {};
        uint32_t curCount = 0;
        VkCommandPool pool = VK_NULL_HANDLE;
    };

private:
    VkCommandBuffer StartCmd(uint32_t frameIndex, AllocatedCmds &cmds, VkQueue queue);

private:
    VkDevice device;

    uint32_t currentFrameIndex;

    const uint32_t cmdAllocStep = 16;

    // allocated cmds
    AllocatedCmds graphicsCmds[MAX_FRAMES_IN_FLIGHT];
    AllocatedCmds computeCmds[MAX_FRAMES_IN_FLIGHT];
    AllocatedCmds transferCmds[MAX_FRAMES_IN_FLIGHT];

    std::weak_ptr<Queues> queues;
    std::map<VkCommandBuffer, VkQueue> cmdQueues[MAX_FRAMES_IN_FLIGHT];
};
