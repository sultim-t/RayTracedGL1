// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "Common.h"
#include "CommandBufferManager.h"
#include "Swapchain.h"

struct GLFWwindow;

namespace RTGL1
{

class DebugWindows : public ISwapchainDependency
{
public:
    DebugWindows( VkInstance            instance,
                  VkPhysicalDevice      physDevice,
                  VkDevice              device,
                  uint32_t              queueFamiy,
                  VkQueue               queue,
                  CommandBufferManager& cmdManager,
                  const Swapchain&      swapchain );
    ~DebugWindows() override;

    void PrepareForFrame( uint32_t frameIndex );
    void Draw();
    void SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex, const Swapchain& swapchain );

    void OnSwapchainCreate(const Swapchain* pSwapchain) override;
    void OnSwapchainDestroy() override;

private:
    VkDevice                     device;

    GLFWwindow*                  glfwWindow;
    VkSurfaceKHR                 glfwSurface;

    VkDescriptorPool             descPool;
    VkRenderPass                 renderPass;
    std::vector< VkFramebuffer > framebuffers;

};

}