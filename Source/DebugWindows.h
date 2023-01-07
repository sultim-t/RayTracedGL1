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

class DebugWindows final : public ISwapchainDependency
{
public:
    DebugWindows( VkInstance                               instance,
                  VkPhysicalDevice                         physDevice,
                  VkDevice                                 device,
                  uint32_t                                 queueFamiy,
                  VkQueue                                  queue,
                  std::shared_ptr< CommandBufferManager >& cmdManager );
    ~DebugWindows() override;

    DebugWindows( const DebugWindows& other )                = delete;
    DebugWindows( DebugWindows&& other ) noexcept            = delete;
    DebugWindows& operator=( const DebugWindows& other )     = delete;
    DebugWindows& operator=( DebugWindows&& other ) noexcept = delete;

    // TODO: remove
    void Init( std::shared_ptr< DebugWindows > self );

    bool PrepareForFrame( uint32_t frameIndex );
    void SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex );
    void OnQueuePresent( VkResult queuePresentResult );

    void OnSwapchainCreate( const Swapchain* pSwapchain ) override;
    void OnSwapchainDestroy() override;

    VkSwapchainKHR GetSwapchainHandle() const;
    uint32_t       GetSwapchainCurrentImageIndex() const;
    VkSemaphore    GetSwapchainImageAvailableSemaphore( uint32_t frameIndex ) const;

    void SetAlwaysOnTop( bool onTop );
    bool IsMinimized() const { return isMinimized; }

private:
    VkDevice device;

    GLFWwindow*                  customWindow;
    VkSurfaceKHR                 customSurface;
    std::unique_ptr< Swapchain > customSwapchain;
    VkSemaphore                  swapchainImageAvailable[ MAX_FRAMES_IN_FLIGHT ];

    VkDescriptorPool             descPool;
    VkRenderPass                 renderPass;
    std::vector< VkFramebuffer > framebuffers;

    bool alwaysOnTop;

    bool isMinimized;
};

}