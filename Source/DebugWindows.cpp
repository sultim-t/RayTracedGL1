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


void RTGL1::DebugWindows::Draw()
{
    static bool  show_demo_window    = true;
    static bool  show_another_window = false;
    static float color[ 3 ]          = { 0.4f, 0.5f, 0.6f };

    static float f       = 0.0f;
    static int   counter = 0;

    ImGui::Begin( "Hello, world!" ); // Create a window called "Hello, world!" and append into it.

    ImGui::Text(
        "This is some useful text." ); // Display some text (you can use a format strings too)
    ImGui::Checkbox( "Demo Window",
                     &show_demo_window ); // Edit bools storing our window open/close state
    ImGui::Checkbox( "Another Window", &show_another_window );

    ImGui::SliderFloat( "float", &f, 0.0f, 1.0f ); // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3( "color", color );           // Edit 3 floats representing a color

    if( ImGui::Button( "Button" ) ) // Buttons return true when clicked (most widgets return
                                    // true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text( "counter = %d", counter );

    ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)",
                 1000.0f / ImGui::GetIO().Framerate,
                 ImGui::GetIO().Framerate );
    ImGui::End();
}


namespace
{

constexpr uint32_t         MIN_IMAGE_COUNT = 2;
constexpr VkPresentModeKHR PRESENT_MODE    = VK_PRESENT_MODE_FIFO_KHR;

void                       glfwErrorCallback( int error, const char* description )
{
    throw RTGL1::RgException( RG_RESULT_GRAPHICS_API_ERROR,
                              "GLFW error (code " + std::to_string( error ) + "): " + description );
}

GLFWwindow* createWindow()
{
    glfwSetErrorCallback( glfwErrorCallback );

    if( !glfwInit() )
    {
        throw RTGL1::RgException( RG_RESULT_GRAPHICS_API_ERROR,
                                  "Failed to initialize GLFW for debug windows" );
    }
    assert( glfwVulkanSupported() );

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    return glfwCreateWindow( 1280, 720, "ImGui window", nullptr, nullptr );
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
    using namespace RTGL1;

    VkAttachmentDescription attchDesc = {
        .format         = swapchainSurfaceFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
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

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, renderPass, VK_OBJECT_TYPE_RENDER_PASS, "ImGui Render pass" );

    return renderPass;
}
} // anonymous

RTGL1::DebugWindows::DebugWindows( VkInstance            _instance,
                                   VkPhysicalDevice      _physDevice,
                                   VkDevice              _device,
                                   uint32_t              _queueFamiy,
                                   VkQueue               _queue,
                                   CommandBufferManager& _cmdManager,
                                   const Swapchain&      _swapchain )
    : device( _device )
    , glfwWindow( createWindow() )
    , glfwSurface( VK_NULL_HANDLE )
    , descPool( CreateDescPool( device ) )
    , renderPass( CreateRenderPass( device, _swapchain.GetSurfaceFormat() ) )
{
    VkResult r = glfwCreateWindowSurface( _instance, glfwWindow, nullptr, &glfwSurface );
    VK_CHECKERROR( r );

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding                = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForVulkan( glfwWindow, true );
    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance        = _instance,
        .PhysicalDevice  = _physDevice,
        .Device          = _device,
        .QueueFamily     = _queueFamiy,
        .Queue           = _queue,
        .PipelineCache   = nullptr,
        .DescriptorPool  = descPool,
        .Subpass         = 0,
        .MinImageCount   = MIN_IMAGE_COUNT,
        .ImageCount      = MIN_IMAGE_COUNT,
        .MSAASamples     = VK_SAMPLE_COUNT_1_BIT,
        .Allocator       = nullptr,
        .CheckVkResultFn = VK_CHECKERROR,
    };
    ImGui_ImplVulkan_Init( &init_info, renderPass );

    UploadFonts( _cmdManager );
}

RTGL1::DebugWindows::~DebugWindows()
{
    vkDeviceWaitIdle( device );

    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyRenderPass( device, renderPass, nullptr );
    for( auto f : framebuffers )
    {
        vkDestroyFramebuffer( device, f, nullptr );
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow( glfwWindow );
    glfwTerminate();
}

void RTGL1::DebugWindows::PrepareForFrame( uint32_t frameIndex )
{
    if( glfwWindowShouldClose( glfwWindow ) )
    {
        return;
    }

    // TODO: check if it runs callbacks 2 times per frame
    glfwPollEvents();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RTGL1::DebugWindows::SubmitForFrame( VkCommandBuffer  cmd,
                                          uint32_t         frameIndex,
                                          const Swapchain& swapchain )
{
    CmdLabel label( cmd, "ImGui" );
    assert( framebuffers.size() == swapchain.GetImageCount() );

    ImGui::Render();

    ImDrawData* mainDrawData = ImGui::GetDrawData();
    assert( mainDrawData );

    if( mainDrawData->DisplaySize.x > 0.0f && mainDrawData->DisplaySize.y > 0.0f )
    {
        VkClearValue clearValue = {
            .color = { .float32 = { 0.45f, 0.55f, 0.60f, 1.00f } },
        };

        VkRenderPassBeginInfo info = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = renderPass,
            .framebuffer     = framebuffers[ swapchain.GetCurrentImageIndex() ],
            .renderArea      = { .extent = { swapchain.GetWidth(), swapchain.GetHeight() } },
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

void RTGL1::DebugWindows::OnSwapchainCreate( const Swapchain* pSwapchain )
{
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

#endif // RG_USE_IMGUI
