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

#include <RTGL1/RTGL1.h>

#include "Common.h"

namespace RTGL1
{

struct FrameState
{
private:
    // [0..MAX_FRAMES_IN_FLIGHT-1]
    uint32_t            frameIndex;
    VkCommandBuffer     frameCmd;
    VkSemaphore         semaphoreToWait;
    // This cmd buffer is used for materials that 
    // are uploaded out of rgStartFrame - rgDrawFrame when
    // 'frameCmd' doesn't exist
    VkCommandBuffer     preFrameCmd;

public:
    FrameState() : 
        frameIndex(MAX_FRAMES_IN_FLIGHT - 1), 
        frameCmd(VK_NULL_HANDLE), 
        semaphoreToWait(VK_NULL_HANDLE),
        preFrameCmd(VK_NULL_HANDLE)
    {}
   
    FrameState(const FrameState &other) = delete;
    FrameState(FrameState &&other) noexcept = delete;
    FrameState &operator=(const FrameState &other) = delete;
    FrameState &operator=(FrameState &&other) noexcept = delete;

    uint32_t IncrementFrameIndexAndGet()
    {
        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        return frameIndex;
    }

    uint32_t GetFrameIndex() const
    {
        assert(frameIndex >= 0 && frameIndex < MAX_FRAMES_IN_FLIGHT);
        return frameIndex;
    }

    static uint32_t GetPrevFrameIndex(uint32_t frameIndex)
    {
        assert(frameIndex >= 0 && frameIndex < MAX_FRAMES_IN_FLIGHT);
        return (frameIndex + (MAX_FRAMES_IN_FLIGHT - 1)) % MAX_FRAMES_IN_FLIGHT;
    }

    void OnBeginFrame(VkCommandBuffer cmd)
    {
        assert(frameCmd == VK_NULL_HANDLE);
        frameCmd = cmd;
    }

    void OnEndFrame()
    {
        assert(frameCmd != VK_NULL_HANDLE);
        // pre-frame cmd must be submitted by this time
        assert(preFrameCmd == VK_NULL_HANDLE);
        frameCmd = VK_NULL_HANDLE;
    }

    VkCommandBuffer GetCmdBuffer() const
    {
        // only in-frame usage
        assert(WasFrameStarted());
        return frameCmd;
    }

    VkCommandBuffer GetCmdBufferForMaterials(const std::shared_ptr<CommandBufferManager> &cmdManager)
    {  
        if (WasFrameStarted())
        {
            // use default cmd buffer, if frame was started
            return GetCmdBuffer();
        }

        // use custom cmd buffer, if out-of-frame call,
        // because the default one doesn't exist yet
        if (preFrameCmd == VK_NULL_HANDLE)
        {
            preFrameCmd = cmdManager->StartGraphicsCmd();
        }

        return preFrameCmd;
    }

    VkCommandBuffer GetPreFrameCmdAndRemove()
    {
        VkCommandBuffer c = preFrameCmd;

        preFrameCmd = VK_NULL_HANDLE;
        return c;
    }

    bool WasFrameStarted() const
    {
        return frameCmd != nullptr;
    }

    void SetSemaphore(VkSemaphore s)
    {
        semaphoreToWait = s;
    }

    VkSemaphore GetSemaphoreForWaitAndRemove()
    {
        VkSemaphore s = semaphoreToWait;

        semaphoreToWait = VK_NULL_HANDLE;
        return s;
    }
};

}