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

#pragma once

#include <span>
#include <vector>

#include "Common.h"
#include "Containers.h"
#include "Queues.h"

namespace RTGL1
{

class CommandBufferManager
{
public:
    explicit CommandBufferManager( VkDevice device, std::shared_ptr< Queues > queues );
    ~CommandBufferManager();

    CommandBufferManager( const CommandBufferManager& other )     = delete;
    CommandBufferManager( CommandBufferManager&& other ) noexcept = delete;
    CommandBufferManager& operator=( const CommandBufferManager& other ) = delete;
    CommandBufferManager& operator=( CommandBufferManager&& other ) noexcept = delete;

    void                  PrepareForFrame( uint32_t frameIndex );

    // Start graphics command buffer for current frame index
    VkCommandBuffer       StartGraphicsCmd();
    // Start compute command buffer for current frame index
    VkCommandBuffer       StartComputeCmd();
    // Start transfer command buffer for current frame index
    VkCommandBuffer       StartTransferCmd();

    void                  Submit( VkCommandBuffer cmd, VkFence fence = VK_NULL_HANDLE );
    void                  Submit( VkCommandBuffer      cmd,
                                  VkSemaphore          waitSemaphore,
                                  VkPipelineStageFlags waitStages,
                                  VkSemaphore          signalSemaphore,
                                  VkFence              fence );
    void                  Submit( VkCommandBuffer             cmd,
                                  const VkSemaphore*          waitSemaphores,
                                  const VkPipelineStageFlags* waitStages,
                                  uint32_t                    waitCount,
                                  VkSemaphore                 signalSemaphore,
                                  VkFence                     fence );


    void                  WaitGraphicsIdle();
    void                  WaitComputeIdle();
    void                  WaitTransferIdle();
    void                  WaitDeviceIdle();

private:
    struct AllocatedCmds
    {
        std::vector< VkCommandBuffer > cmds     = {};
        uint32_t                       curCount = 0;
        VkCommandPool                  pool     = VK_NULL_HANDLE;
    };

private:
    VkCommandBuffer StartCmd( uint32_t frameIndex, AllocatedCmds& cmds, VkQueue queue );

private:
    VkDevice                                       device;

    uint32_t                                       currentFrameIndex;

    const uint32_t                                 cmdAllocStep = 16;

    // allocated cmds
    AllocatedCmds                                  graphicsCmds[ MAX_FRAMES_IN_FLIGHT ];
    AllocatedCmds                                  computeCmds[ MAX_FRAMES_IN_FLIGHT ];
    AllocatedCmds                                  transferCmds[ MAX_FRAMES_IN_FLIGHT ];

    std::shared_ptr< Queues >                      queues;
    rgl::unordered_map< VkCommandBuffer, VkQueue > cmdQueues[ MAX_FRAMES_IN_FLIGHT ];
};

}