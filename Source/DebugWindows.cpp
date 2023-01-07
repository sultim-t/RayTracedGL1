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

#include "DebugWindows.h"

#ifdef RG_USE_IMGUI

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <string>

#include "CmdLabel.h"
#include "RgException.h"

namespace
{

void glfwErrorCallback( int error, const char* description )
{
    throw RTGL1::RgException( RG_RESULT_GRAPHICS_API_ERROR,
                              "GLFW error (code " + std::to_string( error ) + "): " + description );
}

GLFWwindow* CreateGLFWWindow()
{
    glfwSetErrorCallback( glfwErrorCallback );

    if( !glfwInit() )
    {
        throw RTGL1::RgException( RG_RESULT_GRAPHICS_API_ERROR,
                                  "Failed to initialize GLFW for debug windows" );
    }
    assert( glfwVulkanSupported() );

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );
    return glfwCreateWindow( 700, 1000, "RTGL1 Dev", nullptr, nullptr );
}

bool IsSizeNull( GLFWwindow* wnd )
{
    int w, h;
    glfwGetWindowSize( wnd, &w, &h );
    return w == 0 || h == 0;
}

void UploadFonts( RTGL1::CommandBufferManager& cmdManager )
{
    VkCommandBuffer cmd = cmdManager.StartGraphicsCmd();
    {
        ImGui_ImplVulkan_CreateFontsTexture( cmd );
    }
    cmdManager.Submit( cmd );
    cmdManager.WaitGraphicsIdle();

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

uint32_t QueryImageCount( VkPhysicalDevice physDevice, VkSurfaceKHR surface )
{
    VkSurfaceCapabilitiesKHR surfCapabilities;
    VkResult                 r =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physDevice, surface, &surfCapabilities );
    RTGL1::VK_CHECKERROR( r );

    uint32_t imageCount = std::max( 3U, surfCapabilities.minImageCount );

    if( surfCapabilities.maxImageCount > 0 )
    {
        imageCount = std::min( imageCount, surfCapabilities.maxImageCount );
    }

    return imageCount;
}

VkDescriptorPool CreateDescPool( VkDevice device )
{
    using namespace RTGL1;

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000 * std::size( poolSizes ),
        .poolSizeCount = uint32_t( std::size( poolSizes ) ),
        .pPoolSizes    = poolSizes,
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult         r    = vkCreateDescriptorPool( device, &pool_info, nullptr, &pool );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, pool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "ImGui Desc pool" );

    return pool;
}

// Draw directly into the swapchain image
VkRenderPass CreateRenderPass( VkDevice device, VkFormat swapchainSurfaceFormat )
{
    VkAttachmentDescription attchDesc = {
        .format         = swapchainSurfaceFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &attchDesc,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult     r          = vkCreateRenderPass( device, &info, nullptr, &renderPass );

    RTGL1::VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, renderPass, VK_OBJECT_TYPE_RENDER_PASS, "ImGui Render pass" );

    return renderPass;
}

VkSemaphore CreateSwapchainSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore = VK_NULL_HANDLE;
    VkResult    r         = vkCreateSemaphore( device, &semaphoreInfo, nullptr, &semaphore );

    RTGL1::VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, semaphore, VK_OBJECT_TYPE_SEMAPHORE, "ImGui Swapchain image available semaphore" );

    return semaphore;
}

} // anonymous

RTGL1::DebugWindows::DebugWindows( VkInstance                               _instance,
                                   VkPhysicalDevice                         _physDevice,
                                   VkDevice                                 _device,
                                   uint32_t                                 _queueFamiy,
                                   VkQueue                                  _queue,
                                   std::shared_ptr< CommandBufferManager >& _cmdManager )
    : device( _device )
    , customWindow( CreateGLFWWindow() )
    , customSurface( VK_NULL_HANDLE )
    , swapchainImageAvailable{}
    , descPool( CreateDescPool( _device ) )
    , renderPass( VK_NULL_HANDLE )
    , alwaysOnTop( false )
    , isMinimized( false )
{
    VkResult r = glfwCreateWindowSurface( _instance, customWindow, nullptr, &customSurface );
    VK_CHECKERROR( r );

    customSwapchain = std::make_unique< Swapchain >(
        device, customSurface, _physDevice, _cmdManager );

    renderPass = CreateRenderPass( device, customSwapchain->GetSurfaceFormat() );

    for( auto& sm : swapchainImageAvailable )
    {
        sm = CreateSwapchainSemaphore( _device );
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    #if 0 
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    #endif

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding                = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }
    
    ImGui_ImplGlfw_InitForVulkan( customWindow, true );

    uint32_t                  swapchainImageCount = QueryImageCount( _physDevice, customSurface );
    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance        = _instance,
        .PhysicalDevice  = _physDevice,
        .Device          = _device,
        .QueueFamily     = _queueFamiy,
        .Queue           = _queue,
        .PipelineCache   = nullptr,
        .DescriptorPool  = descPool,
        .Subpass         = 0,
        .MinImageCount   = swapchainImageCount,
        .ImageCount      = swapchainImageCount,
        .MSAASamples     = VK_SAMPLE_COUNT_1_BIT,
        .Allocator       = nullptr,
        .CheckVkResultFn = VK_CHECKERROR,
    };
    ImGui_ImplVulkan_Init( &init_info, renderPass );

    UploadFonts( *_cmdManager );
}

RTGL1::DebugWindows::~DebugWindows()
{
    vkDeviceWaitIdle( device );

    for( auto& sm : swapchainImageAvailable )
    {
        vkDestroySemaphore( device, sm, nullptr );
    }
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyRenderPass( device, renderPass, nullptr );
    for( auto f : framebuffers )
    {
        vkDestroyFramebuffer( device, f, nullptr );
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow( customWindow );
    glfwTerminate();
}

void RTGL1::DebugWindows::Init( std::shared_ptr< DebugWindows > self )
{
    // kludge: need a shared_ptr of the current instance
    customSwapchain->Subscribe( std::move( self ) );
}

bool RTGL1::DebugWindows::PrepareForFrame( uint32_t frameIndex )
{
    if( glfwWindowShouldClose( customWindow ) )
    {
        return false;
    }

    glfwPollEvents();

    isMinimized = IsSizeNull( customWindow );
    if( isMinimized )
    {
        return true;
    }

    customSwapchain->AcquireImage( swapchainImageAvailable[ frameIndex ] );

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return true;
}

void RTGL1::DebugWindows::SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    if( isMinimized )
    {
        return;
    }

    CmdLabel label( cmd, "ImGui" );
    assert( framebuffers.size() == customSwapchain->GetImageCount() );

    ImGui::Render();

    ImDrawData* mainDrawData = ImGui::GetDrawData();
    assert( mainDrawData );

    if( mainDrawData->DisplaySize.x > 0.0f && mainDrawData->DisplaySize.y > 0.0f )
    {
        VkClearValue clearValue = {
            .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } },
        };

        VkRenderPassBeginInfo info = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = renderPass,
            .framebuffer     = framebuffers[ customSwapchain->GetCurrentImageIndex() ],
            .renderArea      = { .extent = { customSwapchain->GetWidth(),
                                        customSwapchain->GetHeight() } },
            .clearValueCount = 1,
            .pClearValues    = &clearValue,
        };

        vkCmdBeginRenderPass( cmd, &info, VK_SUBPASS_CONTENTS_INLINE );
        ImGui_ImplVulkan_RenderDrawData( mainDrawData, cmd );
        vkCmdEndRenderPass( cmd );
    }

    if( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void RTGL1::DebugWindows::OnQueuePresent( VkResult queuePresentResult )
{
    if( !isMinimized )
    {
        customSwapchain->OnQueuePresent( queuePresentResult );
    }
}

VkSemaphore RTGL1::DebugWindows::GetSwapchainImageAvailableSemaphore( uint32_t frameIndex ) const
{
    assert( frameIndex < std::size( swapchainImageAvailable ) );
    return swapchainImageAvailable[ frameIndex ];
}

void RTGL1::DebugWindows::SetAlwaysOnTop( bool onTop )
{
    if( alwaysOnTop != onTop )
    {
        alwaysOnTop = onTop;
        
        glfwSetWindowAttrib( customWindow, GLFW_FLOATING, alwaysOnTop ? GLFW_TRUE : GLFW_FALSE );
    }
}

void RTGL1::DebugWindows::OnSwapchainCreate( const Swapchain* pSwapchain )
{
    assert( customSwapchain.get() == pSwapchain );

    assert( framebuffers.empty() );
    framebuffers.clear();

    for( uint32_t i = 0; i < pSwapchain->GetImageCount(); i++ )
    {
        VkImageView             view = pSwapchain->GetImageView( i );

        VkFramebufferCreateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = renderPass,
            .attachmentCount = 1,
            .pAttachments    = &view,
            .width           = pSwapchain->GetWidth(),
            .height          = pSwapchain->GetHeight(),
            .layers          = 1,
        };

        VkFramebuffer fb = VK_NULL_HANDLE;
        VkResult      r  = vkCreateFramebuffer( device, &info, nullptr, &fb );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device, fb, VK_OBJECT_TYPE_FRAMEBUFFER, "ImGui Framebuffer" );

        framebuffers.push_back( fb );
    }
}

void RTGL1::DebugWindows::OnSwapchainDestroy()
{
    assert( !framebuffers.empty() );
    for( auto f : framebuffers )
    {
        vkDestroyFramebuffer( device, f, nullptr );
    }
    framebuffers.clear();
}

VkSwapchainKHR RTGL1::DebugWindows::GetSwapchainHandle() const
{
    return customSwapchain->GetHandle();
}

uint32_t RTGL1::DebugWindows::GetSwapchainCurrentImageIndex() const
{
    return customSwapchain->GetCurrentImageIndex();
}

#endif // RG_USE_IMGUI
