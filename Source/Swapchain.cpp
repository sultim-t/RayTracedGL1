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

#include "Swapchain.h"

#include <algorithm>
#include <utility>

#include "RgException.h"
#include "Utils.h"

namespace
{
bool IsNullExtent( const VkExtent2D& a )
{
    return a.width == 0 || a.height == 0;
}

bool operator==( const VkExtent2D& a, const VkExtent2D& b )
{
    return a.width == b.width && a.height == b.height;
}

bool operator!=( const VkExtent2D& a, const VkExtent2D& b )
{
    return !( a == b );
}
}

RTGL1::Swapchain::Swapchain( VkDevice                                _device,
                             VkSurfaceKHR                            _surface,
                             VkPhysicalDevice                        _physDevice,
                             std::shared_ptr< CommandBufferManager > _cmdManager )
    : device( _device )
    , surface( _surface )
    , physDevice( _physDevice )
    , cmdManager( std::move( _cmdManager ) )
    , surfaceFormat{}
    , presentModeVsync( VK_PRESENT_MODE_FIFO_KHR )
    , presentModeImmediate( VK_PRESENT_MODE_FIFO_KHR )
    , requestedVsync( true )
    , surfaceExtent{ UINT32_MAX, UINT32_MAX }
    , isVsync( true )
    , swapchain( VK_NULL_HANDLE )
    , currentSwapchainIndex( UINT32_MAX )
{
    VkResult r;

    // find surface format
    {
        uint32_t formatCount = 0;
        r = vkGetPhysicalDeviceSurfaceFormatsKHR( physDevice, surface, &formatCount, nullptr );
        VK_CHECKERROR( r );

        std::vector< VkSurfaceFormatKHR > surfaceFormats;
        surfaceFormats.resize( formatCount );

        r = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physDevice, surface, &formatCount, surfaceFormats.data() );
        VK_CHECKERROR( r );

        std::vector< VkFormat > acceptFormats = { VK_FORMAT_R8G8B8A8_SRGB,
                                                  VK_FORMAT_B8G8R8A8_SRGB };

        for( VkFormat f : acceptFormats )
        {
            for( VkSurfaceFormatKHR sf : surfaceFormats )
            {
                if( sf.format == f )
                {
                    surfaceFormat = sf;
                }
            }

            if( surfaceFormat.format != VK_FORMAT_UNDEFINED )
            {
                break;
            }
        }
    }

    // find present modes
    {
        uint32_t presentModeCount = 0;
        r                         = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physDevice, surface, &presentModeCount, nullptr );
        VK_CHECKERROR( r );

        std::vector< VkPresentModeKHR > presentModes( presentModeCount );
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physDevice, surface, &presentModeCount, presentModes.data() );
        VK_CHECKERROR( r );

        // try to find mailbox / fifo-relaxed
        for( auto p : presentModes )
        {
            if( p == VK_PRESENT_MODE_MAILBOX_KHR )
            {
                presentModeImmediate = p;
            }

            if( p == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
            {
                presentModeVsync = p;
            }
        }
    }
}

bool RTGL1::Swapchain::IsExtentOptimal() const
{
    VkSurfaceCapabilitiesKHR surfCapabilities;

    VkResult                 r =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physDevice, surface, &surfCapabilities );

    if( r == VK_ERROR_SURFACE_LOST_KHR )
    {
        return false;
    }

    VK_CHECKERROR( r );

    return !IsNullExtent( surfCapabilities.maxImageExtent ) &&
           !IsNullExtent( surfCapabilities.currentExtent );
}

VkExtent2D RTGL1::Swapchain::GetOptimalExtent() const
{
    VkSurfaceCapabilitiesKHR surfCapabilities;

    VkResult                 r =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physDevice, surface, &surfCapabilities );
    VK_CHECKERROR( r );

    if( IsNullExtent( surfCapabilities.maxImageExtent ) ||
        IsNullExtent( surfCapabilities.currentExtent ) )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_CALL,
                           "Surface has 0 extent. Prevent calling RTGL1 functions in that case" );
    }

    if( surfCapabilities.currentExtent.width == UINT32_MAX ||
        surfCapabilities.currentExtent.height == UINT32_MAX )
    {
        return surfCapabilities.maxImageExtent;
    }

    return surfCapabilities.currentExtent;
}

bool RTGL1::Swapchain::RequestVsync( bool enable )
{
    requestedVsync = enable;
    return requestedVsync != isVsync;
}

void RTGL1::Swapchain::AcquireImage( VkSemaphore imageAvailableSemaphore )
{
    VkExtent2D requestedExtent = GetOptimalExtent();

    // if requested params are different
    if( requestedExtent != surfaceExtent || requestedVsync != isVsync )
    {
        TryRecreate( requestedExtent, requestedVsync );
    }

    while( true )
    {
        VkResult r = vkAcquireNextImageKHR( device,
                                            swapchain,
                                            UINT64_MAX,
                                            imageAvailableSemaphore,
                                            VK_NULL_HANDLE,
                                            &currentSwapchainIndex );

        if( r == VK_SUCCESS )
        {
            break;
        }
        else if( r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR )
        {
            TryRecreate( requestedExtent, requestedVsync );
        }
        else
        {
            assert( 0 );
        }
    }
}

void RTGL1::Swapchain::BlitForPresent( VkCommandBuffer cmd,
                                       VkImage         srcImage,
                                       uint32_t        srcImageWidth,
                                       uint32_t        srcImageHeight,
                                       VkFilter        filter,
                                       VkImageLayout   srcImageLayout )
{
    // if source has almost the same size as the surface, then use nearest blit
    if( std::abs( ( int )srcImageWidth - ( int )surfaceExtent.width ) < 8 &&
        std::abs( ( int )srcImageHeight - ( int )surfaceExtent.height ) < 8 )
    {
        filter = VK_FILTER_NEAREST;
    }


    VkImageBlit region = {};

    region.srcSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.srcOffsets[ 0 ] = { 0, 0, 0 };
    region.srcOffsets[ 1 ] = { static_cast< int32_t >( srcImageWidth ),
                               static_cast< int32_t >( srcImageHeight ),
                               1 };

    region.dstSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstOffsets[ 0 ] = { 0, 0, 0 };
    region.dstOffsets[ 1 ] = { static_cast< int32_t >( surfaceExtent.width ),
                               static_cast< int32_t >( surfaceExtent.height ),
                               1 };

    VkImage       swapchainImage       = swapchainImages[ currentSwapchainIndex ];
    VkImageLayout swapchainImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // set layout for blit
    Utils::BarrierImage( cmd,
                         srcImage,
                         VK_ACCESS_SHADER_WRITE_BIT,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         srcImageLayout,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );

    Utils::BarrierImage( cmd,
                         swapchainImage,
                         0,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         swapchainImageLayout,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

    vkCmdBlitImage( cmd,
                    srcImage,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    swapchainImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &region,
                    filter );

    // restore layouts
    Utils::BarrierImage( cmd,
                         srcImage,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         srcImageLayout );

    Utils::BarrierImage( cmd,
                         swapchainImage,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         0,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         swapchainImageLayout );
}

void RTGL1::Swapchain::Present( const std::shared_ptr< Queues >& queues,
                                VkSemaphore                      renderFinishedSemaphore )
{
    VkPresentInfoKHR presentInfo   = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain;
    presentInfo.pImageIndices      = &currentSwapchainIndex;
    presentInfo.pResults           = nullptr;

    VkResult r = vkQueuePresentKHR( queues->GetGraphics(), &presentInfo );

    if( r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR )
    {
        TryRecreate( GetOptimalExtent(), requestedVsync );
    }
}

bool RTGL1::Swapchain::TryRecreate( const VkExtent2D& newExtent, bool vsync )
{
    if( surfaceExtent == newExtent && isVsync == vsync )
    {
        return false;
    }

    cmdManager->WaitDeviceIdle();

    VkSwapchainKHR old = DestroyWithoutSwapchain();
    Create( newExtent.width, newExtent.height, vsync, old );

    return true;
}

void RTGL1::Swapchain::Create( uint32_t       newWidth,
                               uint32_t       newHeight,
                               bool           vsync,
                               VkSwapchainKHR oldSwapchain )
{
    this->isVsync       = vsync;
    this->surfaceExtent = { newWidth, newHeight };

    VkSurfaceCapabilitiesKHR surfCapabilities;
    VkResult                 r =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physDevice, surface, &surfCapabilities );
    VK_CHECKERROR( r );

    if( surfCapabilities.currentExtent.width != UINT32_MAX &&
        surfCapabilities.currentExtent.height != UINT32_MAX )
    {
        assert( surfaceExtent == surfCapabilities.currentExtent );
    }
    else
    {
        assert( surfCapabilities.minImageExtent.width <= surfaceExtent.width &&
                surfaceExtent.width <= surfCapabilities.maxImageExtent.width );
        assert( surfCapabilities.minImageExtent.height <= surfaceExtent.height &&
                surfaceExtent.height <= surfCapabilities.maxImageExtent.height );
    }

    assert( swapchain == VK_NULL_HANDLE );
    assert( swapchainImages.empty() );
    assert( swapchainViews.empty() );

    uint32_t imageCount = std::max( 3U, surfCapabilities.minImageCount );
    if( surfCapabilities.maxImageCount > 0 )
    {
        imageCount = std::min( imageCount, surfCapabilities.maxImageCount );
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface                  = surface;
    swapchainInfo.minImageCount            = imageCount;
    swapchainInfo.imageFormat              = surfaceFormat.format;
    swapchainInfo.imageColorSpace          = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent              = surfaceExtent;
    swapchainInfo.imageArrayLayers         = 1;
    swapchainInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform     = surfCapabilities.currentTransform;
    swapchainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode      = vsync ? presentModeVsync : presentModeImmediate;
    swapchainInfo.clipped          = VK_FALSE;
    swapchainInfo.oldSwapchain     = oldSwapchain;

    r = vkCreateSwapchainKHR( device, &swapchainInfo, nullptr, &swapchain );
    VK_CHECKERROR( r );

    if( oldSwapchain != VK_NULL_HANDLE )
    {
        vkDestroySwapchainKHR( device, oldSwapchain, nullptr );
    }

    r = vkGetSwapchainImagesKHR( device, swapchain, &imageCount, nullptr );
    VK_CHECKERROR( r );

    swapchainImages.resize( imageCount );
    swapchainViews.resize( imageCount );

    r = vkGetSwapchainImagesKHR( device, swapchain, &imageCount, swapchainImages.data() );
    VK_CHECKERROR( r );

    for( uint32_t i = 0; i < imageCount; i++ )
    {
        VkImageViewCreateInfo viewInfo           = {};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = swapchainImages[ i ];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = surfaceFormat.format;
        viewInfo.components                      = {};
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        r = vkCreateImageView( device, &viewInfo, nullptr, &swapchainViews[ i ] );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, swapchainImages[ i ], VK_OBJECT_TYPE_IMAGE, "Swapchain image" );
        SET_DEBUG_NAME(
            device, swapchainViews[ i ], VK_OBJECT_TYPE_IMAGE_VIEW, "Swapchain image view" );
    }

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    for( uint32_t i = 0; i < imageCount; i++ )
    {
        Utils::BarrierImage( cmd,
                             swapchainImages[ i ],
                             0,
                             0,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
    }

    cmdManager->Submit( cmd );
    cmdManager->WaitGraphicsIdle();

    CallCreateSubscribers();
}

void RTGL1::Swapchain::Destroy()
{
    VkSwapchainKHR old = DestroyWithoutSwapchain();
    vkDestroySwapchainKHR( device, old, nullptr );
}

VkSwapchainKHR RTGL1::Swapchain::DestroyWithoutSwapchain()
{
    vkDeviceWaitIdle( device );

    if( swapchain != VK_NULL_HANDLE )
    {
        CallDestroySubscribers();
    }

    for( VkImageView v : swapchainViews )
    {
        vkDestroyImageView( device, v, nullptr );
    }

    swapchainViews.clear();
    swapchainImages.clear();

    VkSwapchainKHR old = swapchain;
    swapchain          = VK_NULL_HANDLE;

    return old;
}

void RTGL1::Swapchain::CallCreateSubscribers()
{
    for( auto& ws : subscribers )
    {
        if( auto s = ws.lock() )
        {
            s->OnSwapchainCreate( this );
        }
    }
}

void RTGL1::Swapchain::CallDestroySubscribers()
{
    for( auto& ws : subscribers )
    {
        if( auto s = ws.lock() )
        {
            s->OnSwapchainDestroy();
        }
    }
}

RTGL1::Swapchain::~Swapchain()
{
    Destroy();
}

void RTGL1::Swapchain::Subscribe( std::shared_ptr< ISwapchainDependency > subscriber )
{
    subscribers.emplace_back( subscriber );
}

void RTGL1::Swapchain::Unsubscribe( const ISwapchainDependency* subscriber )
{
    subscribers.remove_if( [ subscriber ]( const std::weak_ptr< ISwapchainDependency >& ws ) {
        if( const auto s = ws.lock() )
        {
            return s.get() == subscriber;
        }

        return true;
    } );
}

VkFormat RTGL1::Swapchain::GetSurfaceFormat() const
{
    return surfaceFormat.format;
}

uint32_t RTGL1::Swapchain::GetWidth() const
{
    return surfaceExtent.width;
}

uint32_t RTGL1::Swapchain::GetHeight() const
{
    return surfaceExtent.height;
}

uint32_t RTGL1::Swapchain::GetCurrentImageIndex() const
{
    return currentSwapchainIndex;
}

uint32_t RTGL1::Swapchain::GetImageCount() const
{
    assert( swapchainViews.size() == swapchainImages.size() );
    return uint32_t( swapchainViews.size() );
}

VkImageView RTGL1::Swapchain::GetImageView( uint32_t index ) const
{
    assert( index < swapchainViews.size() );
    return swapchainViews[ index ];
}

VkImage RTGL1::Swapchain::GetImage( uint32_t index ) const
{
    assert( index < swapchainImages.size() );
    return swapchainImages[ index ];
}

const VkImageView* RTGL1::Swapchain::GetImageViews() const
{
    if( !swapchainViews.empty() )
    {
        return swapchainViews.data();
    }

    return nullptr;
}
