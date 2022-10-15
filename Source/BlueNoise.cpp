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

#include "BlueNoise.h"

#include <string>

#include "Generated/ShaderCommonC.h"
#include "ImageLoader.h"
#include "Utils.h"
#include "RgException.h"

namespace
{
// no compression
constexpr VkFormat     IMAGE_FORMAT    = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkDeviceSize BYTES_PER_PIXEL = 4;
}

RTGL1::BlueNoise::BlueNoise( VkDevice                                       _device,
                             const char*                                    _blueNoiseFilePath,
                             std::shared_ptr< MemoryAllocator >             _allocator,
                             const std::shared_ptr< CommandBufferManager >& _cmdManager,
                             std::shared_ptr< UserFileLoad >                _userFileLoad )
    : device( _device )
    , allocator( std::move( _allocator ) )
    , blueNoiseImages( VK_NULL_HANDLE )
    , blueNoiseImagesView( VK_NULL_HANDLE )
    , descSetLayout( VK_NULL_HANDLE )
    , descPool( VK_NULL_HANDLE )
    , descSet( VK_NULL_HANDLE )
{
    using namespace std::string_literals;

    constexpr VkDeviceSize oneLayerSize =
        BYTES_PER_PIXEL * BLUE_NOISE_TEXTURE_SIZE * BLUE_NOISE_TEXTURE_SIZE;
    constexpr VkDeviceSize dataSize = oneLayerSize * BLUE_NOISE_TEXTURE_COUNT;


    ImageLoader            imageLoader( std::move( _userFileLoad ) );
    const auto             resultInfo = imageLoader.LoadLayered( _blueNoiseFilePath );

    if( !resultInfo )
    {
        throw RgException( RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES,
                           "Can't find blue noise file: "s + _blueNoiseFilePath );
    }

    if( resultInfo->baseSize.width != BLUE_NOISE_TEXTURE_SIZE ||
        resultInfo->baseSize.height != BLUE_NOISE_TEXTURE_SIZE )
    {
        throw RgException( RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES,
                           "Blue noise image size must be " +
                               std::to_string( BLUE_NOISE_TEXTURE_SIZE ) );
    }

    if( resultInfo->layerData.size() != BLUE_NOISE_TEXTURE_COUNT )
    {
        throw RgException( RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES,
                           "Blue noise image must have " +
                               std::to_string( BLUE_NOISE_TEXTURE_COUNT ) + " layers" );
    }

    if( resultInfo->format != IMAGE_FORMAT )
    {
        throw RgException( RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES,
                           "Blue noise image must have R8G8B8A8_UNORM format" );
    }

    // allocate buffer for all textures
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size               = dataSize;
    stagingInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


    void*    mappedData    = nullptr;
    VkBuffer stagingBuffer = allocator->CreateStagingSrcTextureBuffer(
        &stagingInfo, "Blue noise image VMA staging alloc", &mappedData );

    assert( stagingBuffer != VK_NULL_HANDLE );

    // load each texture and place it in staging buffer
    for( uint32_t i = 0; i < BLUE_NOISE_TEXTURE_COUNT; i++ )
    {
        void* dst = static_cast< uint8_t* >( mappedData ) + oneLayerSize * i;

        memcpy( dst, resultInfo->layerData[ i ], oneLayerSize );
    }

    imageLoader.FreeLoaded();


    // create image that contains all blue noise textures as layers
    VkImageCreateInfo info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = IMAGE_FORMAT,
        .extent        = { BLUE_NOISE_TEXTURE_SIZE, BLUE_NOISE_TEXTURE_SIZE, 1 },
        .mipLevels     = 1,
        .arrayLayers   = BLUE_NOISE_TEXTURE_COUNT,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    blueNoiseImages = allocator->CreateDstTextureImage( &info, "Blue noise image VMA alloc" );
    SET_DEBUG_NAME( device, blueNoiseImages, VK_OBJECT_TYPE_IMAGE, "Blue noise Image" );

    // copy from buffer to image
    VkCommandBuffer         cmd = _cmdManager->StartGraphicsCmd();

    VkImageSubresourceRange allLayersRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = BLUE_NOISE_TEXTURE_COUNT,
    };

    // to dst layout
    Utils::BarrierImage( cmd,
                         blueNoiseImages,
                         0,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         allLayersRange );

    VkBufferImageCopy copyInfo = {
        .bufferOffset = 0,
        // tigthly packed
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = BLUE_NOISE_TEXTURE_COUNT,
        },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { BLUE_NOISE_TEXTURE_SIZE, BLUE_NOISE_TEXTURE_SIZE, 1 },
    };

    vkCmdCopyBufferToImage(
        cmd, stagingBuffer, blueNoiseImages, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo );

    // to read in shaders
    Utils::BarrierImage( cmd,
                         blueNoiseImages,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         allLayersRange );

    // submit and wait
    _cmdManager->Submit( cmd );
    _cmdManager->WaitGraphicsIdle();

    allocator->DestroyStagingSrcTextureBuffer( stagingBuffer );

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = blueNoiseImages,
        // multi-layer image
        .viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format           = IMAGE_FORMAT,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = BLUE_NOISE_TEXTURE_COUNT,
        },
    };

    VkResult r = vkCreateImageView( device, &viewInfo, nullptr, &blueNoiseImagesView );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, blueNoiseImagesView, VK_OBJECT_TYPE_IMAGE_VIEW, "Blue noise View" );

    CreateDescriptors();
}

RTGL1::BlueNoise::~BlueNoise()
{
    vkDestroyDescriptorSetLayout( device, descSetLayout, nullptr );
    vkDestroyDescriptorPool( device, descPool, nullptr );

    vkDestroyImageView( device, blueNoiseImagesView, nullptr );
    allocator->DestroyTextureImage( blueNoiseImages );
}

VkDescriptorSetLayout RTGL1::BlueNoise::GetDescSetLayout() const
{
    return descSetLayout;
}

VkDescriptorSet RTGL1::BlueNoise::GetDescSet() const
{
    return descSet;
}

void RTGL1::BlueNoise::CreateDescriptors()
{
    VkResult                     r;

    VkDescriptorSetLayoutBinding binding = {
        .binding         = BINDING_BLUE_NOISE,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_ALL,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &binding,
    };

    r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &descSetLayout );
    VK_CHECKERROR( r );

    VkDescriptorPoolSize poolSize = {
        .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = 1,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };

    r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );
    VK_CHECKERROR( r );

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &descSetLayout,
    };

    r = vkAllocateDescriptorSets( device, &allocInfo, &descSet );
    VK_CHECKERROR( r );

    VkDescriptorImageInfo imgInfo = {
        .imageView   = blueNoiseImagesView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descSet,
        .dstBinding      = BINDING_BLUE_NOISE,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo      = &imgInfo,
    };

    vkUpdateDescriptorSets( device, 1, &write, 0, nullptr );

    SET_DEBUG_NAME(
        device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Blue noise Desc set layout" );
    SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Blue noise Desc pool" );
    SET_DEBUG_NAME( device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Blue noise Desc set" );
}
