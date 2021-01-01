#include "CommandBufferManager.h"
#include "Utils.h"

CommandBufferManager::CommandBufferManager(VkDevice device, std::shared_ptr<Queues> queues) :
    currentFrameIndex(0)
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
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &graphicsCmds[i].pool);
        VK_CHECKERROR(r);

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexCompute();
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &computeCmds[i].pool);
        VK_CHECKERROR(r);

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexTransfer();
        r = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &transferCmds[i].pool);
        VK_CHECKERROR(r);
    }
}

CommandBufferManager::~CommandBufferManager()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(cmdQueues[i].empty());

        vkDestroyCommandPool(device, graphicsCmds[i].pool, nullptr);
        vkDestroyCommandPool(device, computeCmds[i].pool, nullptr);
        vkDestroyCommandPool(device, transferCmds[i].pool, nullptr);
    }
}

void CommandBufferManager::PrepareForFrame(uint32_t frameIndex)
{
    vkResetCommandPool(device, graphicsCmds[frameIndex].pool, 0);
    vkResetCommandPool(device, computeCmds[frameIndex].pool, 0);
    vkResetCommandPool(device, transferCmds[frameIndex].pool, 0);

    currentFrameIndex = frameIndex;

    assert(cmdQueues[frameIndex].empty());
}

VkCommandBuffer CommandBufferManager::StartCmd(uint32_t frameIndex, AllocatedCmds &allocated, VkQueue queue)
{
    VkResult r;

    uint32_t oldCount = allocated.cmds.size();

    // if not enough, allocate new buffers
    if (allocated.curCount + 1 > oldCount)
    {
        allocated.cmds.resize(oldCount + cmdAllocStep);

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = allocated.pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = cmdAllocStep;

        r = vkAllocateCommandBuffers(device, &allocInfo, &allocated.cmds[oldCount]);
        VK_CHECKERROR(r);
    }

    VkCommandBuffer cmd = allocated.cmds[allocated.curCount];
    allocated.curCount++;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    r = vkBeginCommandBuffer(cmd, &beginInfo);
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

    return StartCmd(currentFrameIndex, graphicsCmds[currentFrameIndex], queues.lock()->GetGraphics());
}

VkCommandBuffer CommandBufferManager::StartComputeCmd()
{
    if (queues.expired())
    {
        return VK_NULL_HANDLE;
    }

    return StartCmd(currentFrameIndex, computeCmds[currentFrameIndex], queues.lock()->GetCompute());
}

VkCommandBuffer CommandBufferManager::StartTransferCmd()
{
    if (queues.expired())
    {
        return VK_NULL_HANDLE;
    }

    return StartCmd(currentFrameIndex, transferCmds[currentFrameIndex], queues.lock()->GetTransfer());
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

void CommandBufferManager::WaitForFence(VkFence fence)
{
    VkResult r = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    VK_CHECKERROR(r);
}

void CommandBufferManager::WaitGraphicsIdle()
{
    if (auto qs = queues.lock())
    {
        VkResult r = vkQueueWaitIdle(qs->GetGraphics());
        VK_CHECKERROR(r);
    }
}

void CommandBufferManager::WaitComputeIdle()
{
    if (auto qs = queues.lock())
    {
        VkResult r = vkQueueWaitIdle(qs->GetCompute());
        VK_CHECKERROR(r);
    }
}

void CommandBufferManager::WaitTransferIdle()
{
    if (auto qs = queues.lock())
    {
        VkResult r = vkQueueWaitIdle(qs->GetTransfer());
        VK_CHECKERROR(r);
    }
}

void CommandBufferManager::WaitDeviceIdle()
{
    vkDeviceWaitIdle(device);
}
