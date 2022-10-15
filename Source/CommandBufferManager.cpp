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

#include "CommandBufferManager.h"
#include "Utils.h"

RTGL1::CommandBufferManager::CommandBufferManager( VkDevice                  _device,
                                                   std::shared_ptr< Queues > _queues )
    : device( _device )
    , currentFrameIndex( MAX_FRAMES_IN_FLIGHT - 1 )
    , queues( std::move( _queues ) )
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags                   = 0;

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        VkResult r;

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexGraphics();
        r = vkCreateCommandPool( device, &cmdPoolInfo, nullptr, &graphicsCmds[ i ].pool );
        VK_CHECKERROR( r );

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexCompute();
        r = vkCreateCommandPool( device, &cmdPoolInfo, nullptr, &computeCmds[ i ].pool );
        VK_CHECKERROR( r );

        cmdPoolInfo.queueFamilyIndex = queues->GetIndexTransfer();
        r = vkCreateCommandPool( device, &cmdPoolInfo, nullptr, &transferCmds[ i ].pool );
        VK_CHECKERROR( r );
    }
}

RTGL1::CommandBufferManager::~CommandBufferManager()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( cmdQueues[ i ].empty() );

        vkDestroyCommandPool( device, graphicsCmds[ i ].pool, nullptr );
        vkDestroyCommandPool( device, computeCmds[ i ].pool, nullptr );
        vkDestroyCommandPool( device, transferCmds[ i ].pool, nullptr );
    }
}

void RTGL1::CommandBufferManager::PrepareForFrame( uint32_t frameIndex )
{
    assert( cmdQueues[ frameIndex ].empty() );

    vkResetCommandPool( device, graphicsCmds[ frameIndex ].pool, 0 );
    vkResetCommandPool( device, computeCmds[ frameIndex ].pool, 0 );
    vkResetCommandPool( device, transferCmds[ frameIndex ].pool, 0 );

    graphicsCmds[ frameIndex ].curCount = 0;
    computeCmds[ frameIndex ].curCount  = 0;
    transferCmds[ frameIndex ].curCount = 0;

    currentFrameIndex = frameIndex;
}

VkCommandBuffer RTGL1::CommandBufferManager::StartCmd( uint32_t       frameIndex,
                                                       AllocatedCmds& allocated,
                                                       VkQueue        queue )
{
    VkResult r;

    size_t oldCount = allocated.cmds.size();

    // if not enough, allocate new buffers
    if( allocated.curCount + 1 > oldCount )
    {
        allocated.cmds.resize( oldCount + cmdAllocStep );

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool                 = allocated.pool;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount          = cmdAllocStep;

        r = vkAllocateCommandBuffers( device, &allocInfo, &allocated.cmds[ oldCount ] );
        VK_CHECKERROR( r );
    }

    VkCommandBuffer cmd = allocated.cmds[ allocated.curCount ];
    allocated.curCount++;

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    r = vkBeginCommandBuffer( cmd, &beginInfo );
    VK_CHECKERROR( r );

    cmdQueues[ frameIndex ][ cmd ] = queue;

    return cmd;
}

VkCommandBuffer RTGL1::CommandBufferManager::StartGraphicsCmd()
{
    return StartCmd(
        currentFrameIndex, graphicsCmds[ currentFrameIndex ], queues->GetGraphics() );
}

VkCommandBuffer RTGL1::CommandBufferManager::StartComputeCmd()
{
    return StartCmd(
        currentFrameIndex, computeCmds[ currentFrameIndex ], queues->GetCompute() );
}

VkCommandBuffer RTGL1::CommandBufferManager::StartTransferCmd()
{
    return StartCmd(
        currentFrameIndex, transferCmds[ currentFrameIndex ], queues->GetTransfer() );
}

void RTGL1::CommandBufferManager::Submit( VkCommandBuffer cmd, VkFence fence )
{
    VkResult r = vkEndCommandBuffer( cmd );
    VK_CHECKERROR( r );

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };

    assert( cmdQueues[ currentFrameIndex ].find( cmd ) != cmdQueues[ currentFrameIndex ].end() );

    auto& qs = cmdQueues[ currentFrameIndex ];
    assert( qs.find( cmd ) != qs.end() );

    VkQueue q = qs[ cmd ];
    qs.erase( cmd );

    r = vkQueueSubmit( q, 1, &submitInfo, fence );
    VK_CHECKERROR( r );
}

void RTGL1::CommandBufferManager::Submit( VkCommandBuffer      cmd,
                                          VkSemaphore          waitSemaphore,
                                          VkPipelineStageFlags waitStages,
                                          VkSemaphore          signalSemaphore,
                                          VkFence              fence )
{
    VkResult r = vkEndCommandBuffer( cmd );
    VK_CHECKERROR( r );

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &waitSemaphore,
        .pWaitDstStageMask    = &waitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &signalSemaphore,
    };

    auto& qs = cmdQueues[ currentFrameIndex ];
    assert( qs.find( cmd ) != qs.end() );

    VkQueue q = qs[ cmd ];
    qs.erase( cmd );

    r = vkQueueSubmit( q, 1, &submitInfo, fence );
    VK_CHECKERROR( r );
}


void RTGL1::CommandBufferManager::WaitGraphicsIdle()
{
    VkResult r = vkQueueWaitIdle( queues->GetGraphics() );
    VK_CHECKERROR( r );
}

void RTGL1::CommandBufferManager::WaitComputeIdle()
{
    VkResult r = vkQueueWaitIdle( queues->GetCompute() );
    VK_CHECKERROR( r );
}

void RTGL1::CommandBufferManager::WaitTransferIdle()
{
    VkResult r = vkQueueWaitIdle( queues->GetTransfer() );
    VK_CHECKERROR( r );
}

void RTGL1::CommandBufferManager::WaitDeviceIdle()
{
    vkDeviceWaitIdle( device );
}
