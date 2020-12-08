#include "CommandBufferManager.h"

CommandBufferManager::CommandBufferManager(VkDevice device, std::shared_ptr<Queues> queues)
{
    this->device = device;
    this->queues = queues;

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkResult r;

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexGraphics();
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &graphicsPools[i]);
        VK_CHECKERROR(r);

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexCompute();
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &computePools[i]);
        VK_CHECKERROR(r);

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexTransfer();
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &transferPools[i]);
        VK_CHECKERROR(r);
    }
}

CommandBufferManager::~CommandBufferManager()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyCommandPool(device, graphicsPools[i], nullptr);
        vkDestroyCommandPool(device, computePools[i], nullptr);
        vkDestroyCommandPool(device, transferPools[i], nullptr);
    }
}

void CommandBufferManager::PrepareForFrame(uint32_t frameIndex)
{
    vkResetCommandPool(device, graphicsPools[frameIndex], 0);
    vkResetCommandPool(device, computePools[frameIndex], 0);
    vkResetCommandPool(device, transferPools[frameIndex], 0);

    cmdQueues[frameIndex].clear();
}

VkCommandBuffer CommandBufferManager::StartCmd(uint32_t frameIndex, VkCommandPool cmdPool, VkQueue queue)
{
    VkCommandBuffer cmd;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult r = vkAllocateCommandBuffers(device, &allocInfo, &cmd);
    VK_CHECKERROR(r);

    cmdQueues[frameIndex][cmd] = queue;

    return cmd;
}

VkCommandBuffer CommandBufferManager::StartGraphicsCmd()
{
    if (queues.expired())
    {
        return VK_NULL_HANDLE;
    }

    return StartCmd(currentFrameIndex, graphicsPools[currentFrameIndex], queues.lock()->GetGraphics());
}

VkCommandBuffer CommandBufferManager::StartComputeCmd()
{
    if (queues.expired())
    {
        return VK_NULL_HANDLE;
    }

    return StartCmd(currentFrameIndex, computePools[currentFrameIndex], queues.lock()->GetCompute());
}

VkCommandBuffer CommandBufferManager::StartTransferCmd()
{
    if (queues.expired())
    {
        return VK_NULL_HANDLE;
    }

    return StartCmd(currentFrameIndex, transferPools[currentFrameIndex], queues.lock()->GetTransfer());
}

void CommandBufferManager::Submit(VkCommandBuffer cmd, VkFence fence)
{
    VkResult r = vkEndCommandBuffer(cmd);
    VK_CHECKERROR(r);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    assert(cmdQueues[currentFrameIndex].find(cmd) != cmdQueues[currentFrameIndex].end());

    auto &qs = cmdQueues[currentFrameIndex];
    assert(qs.find(cmd) != qs.end());

    VkQueue q = qs[cmd];
    qs.erase(cmd);

    r = vkQueueSubmit(q, 1, &submitInfo, fence);
    VK_CHECKERROR(r);
}

void CommandBufferManager::Submit(VkCommandBuffer cmd, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages,
    VkSemaphore signalSemaphore, VkFence fence)
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

    auto &qs = cmdQueues[currentFrameIndex];
    assert(qs.find(cmd) != qs.end());

    VkQueue q = qs[cmd];
    qs.erase(cmd);

    r = vkQueueSubmit(q, 1, &submitInfo, fence);
    VK_CHECKERROR(r);
}
