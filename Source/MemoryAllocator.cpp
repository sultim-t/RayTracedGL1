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

#include "MemoryAllocator.h"

#include "Const.h"

RTGL1::MemoryAllocator::MemoryAllocator( VkInstance                        _instance,
                                         VkDevice                          _device,
                                         std::shared_ptr< PhysicalDevice > _physDevice )
    : device( _device )
    , physDevice( std::move( _physDevice ) )
    , allocator( VK_NULL_HANDLE )
    , texturesStagingPool( VK_NULL_HANDLE )
    , texturesFinalPool( VK_NULL_HANDLE )
{
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | // currently, the library uses
                                                                    // only one thread,
                 VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT, // if buffer/image requires a
                                                                    // dedicated allocation
        .physicalDevice   = physDevice->Get(),
        .device           = device,
        .frameInUseCount  = MAX_FRAMES_IN_FLIGHT,
        .instance         = _instance,
        .vulkanApiVersion = VK_API_VERSION_1_2,
    };

    VkResult r = vmaCreateAllocator( &allocatorInfo, &allocator );
    VK_CHECKERROR( r );

    CreateTexturesStagingPool();
    CreateTexturesFinalPool();
}

RTGL1::MemoryAllocator::~MemoryAllocator()
{
    assert( bufAllocs.empty() );

    vmaDestroyPool( allocator, texturesStagingPool );
    vmaDestroyPool( allocator, texturesFinalPool );
    vmaDestroyAllocator( allocator );
}

VkBuffer RTGL1::MemoryAllocator::CreateStagingSrcTextureBuffer( const VkBufferCreateInfo* info,
                                                                const char*     pDebugName,
                                                                void**          pOutMappedData,
                                                                VkDeviceMemory* outMemory )
{
    // alloc TRANSFER_SRC buffer with writeable by CPU memory
    VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT,
        .pool  = texturesStagingPool,
        .pUserData = const_cast< char* >( pDebugName ),
    };
    VkBuffer          buffer;
    VmaAllocation     resultAlloc;
    VmaAllocationInfo resultAllocInfo = {};

    VkResult          r =
        vmaCreateBuffer( allocator, info, &allocInfo, &buffer, &resultAlloc, &resultAllocInfo );

    VK_CHECKERROR( r );
    if( r != VK_SUCCESS )
    {
        return VK_NULL_HANDLE;
    }

    if( buffer == VK_NULL_HANDLE )
    {
        return VK_NULL_HANDLE;
    }

    bufAllocs[ buffer ] = resultAlloc;

    if( outMemory != nullptr )
    {
        *outMemory = resultAllocInfo.deviceMemory;
    }

    *pOutMappedData = resultAllocInfo.pMappedData;
    return buffer;
}

VkImage RTGL1::MemoryAllocator::CreateDstTextureImage( const VkImageCreateInfo* info,
                                                       const char*              pDebugName,
                                                       VkDeviceMemory*          outMemory )
{
    // alloc SAMPLED_BIT | TRANSFER_DST
    VmaAllocationCreateInfo allocInfo = {
        .flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT,
        .pool      = texturesFinalPool,
        .pUserData = const_cast< char* >( pDebugName ),
    };

    VkImage           image;
    VmaAllocation     resultAlloc;
    VmaAllocationInfo resultAllocInfo = {};

    VkResult          r =
        vmaCreateImage( allocator, info, &allocInfo, &image, &resultAlloc, &resultAllocInfo );

    VK_CHECKERROR( r );
    if( r != VK_SUCCESS )
    {
        return VK_NULL_HANDLE;
    }

    if( image == VK_NULL_HANDLE )
    {
        return VK_NULL_HANDLE;
    }

    imgAllocs[ image ] = resultAlloc;

    if( outMemory != nullptr )
    {
        *outMemory = resultAllocInfo.deviceMemory;
    }

    return image;
}

void RTGL1::MemoryAllocator::DestroyStagingSrcTextureBuffer( VkBuffer buffer )
{
    if( bufAllocs.find( buffer ) == bufAllocs.end() )
    {
        assert( 0 );
        return;
    }

    vmaDestroyBuffer( allocator, buffer, bufAllocs[ buffer ] );
    bufAllocs.erase( buffer );
}

void RTGL1::MemoryAllocator::DestroyTextureImage( VkImage image )
{
    if( imgAllocs.find( image ) == imgAllocs.end() )
    {
        assert( 0 );
        return;
    }

    vmaDestroyImage( allocator, image, imgAllocs[ image ] );
    imgAllocs.erase( image );
}

void RTGL1::MemoryAllocator::CreateTexturesStagingPool()
{
    VkResult           r;

    // Vma will create and destroy it for identifying the memory type index
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = 64,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    // transfer source, will be filled from the cpu
    VmaAllocationCreateInfo prototype = {
        .flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT,
        .usage     = VMA_MEMORY_USAGE_CPU_ONLY,
        .pUserData = const_cast< char* >( "VMA Image staing pool prototype" ),
    };

    uint32_t memTypeIndex;
    r = vmaFindMemoryTypeIndexForBufferInfo( allocator, &bufferInfo, &prototype, &memTypeIndex );
    VK_CHECKERROR( r );

    VmaPoolCreateInfo poolInfo = {};
    poolInfo.frameInUseCount   = MAX_FRAMES_IN_FLIGHT;
    poolInfo.memoryTypeIndex   = memTypeIndex;
    poolInfo.blockSize         = ALLOCATOR_BLOCK_SIZE_STAGING_TEXTURES;
    // buddy algorithm as textures has commonly a size of power of 2
    poolInfo.flags = VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT;

    r = vmaCreatePool( allocator, &poolInfo, &texturesStagingPool );
    VK_CHECKERROR( r );
}

void RTGL1::MemoryAllocator::CreateTexturesFinalPool()
{
    VkResult          r;

    // Vma will create and destroy it for identifying the memory type index
    VkImageCreateInfo imageInfo = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R8G8B8A8_SRGB,
        .extent      = { 1, 1, 1 },
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };

    // transfer destination, data will be copied from staging buffer
    VmaAllocationCreateInfo prototype = {
        .flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT,
        .usage     = VMA_MEMORY_USAGE_GPU_ONLY,
        .pUserData = const_cast< char* >( "VMA Image pool prototype" ),
    };

    uint32_t memTypeIndex;
    r = vmaFindMemoryTypeIndexForImageInfo( allocator, &imageInfo, &prototype, &memTypeIndex );
    VK_CHECKERROR( r );

    VmaPoolCreateInfo poolInfo = {
        .memoryTypeIndex = memTypeIndex,
        // buddy algorithm as textures has commonly a size of power of 2
        .flags           = VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT,
        .blockSize       = ALLOCATOR_BLOCK_SIZE_TEXTURES,
        .frameInUseCount = MAX_FRAMES_IN_FLIGHT,
    };

    r = vmaCreatePool( allocator, &poolInfo, &texturesFinalPool );
    VK_CHECKERROR( r );
}

VkDevice RTGL1::MemoryAllocator::GetDevice()
{
    return device;
}

VkPhysicalDevice RTGL1::MemoryAllocator::GetPhysicalDevice()
{
    return physDevice->Get();
}

VkDeviceMemory RTGL1::MemoryAllocator::AllocDedicated( const VkMemoryRequirements& memReqs,
                                                       VkMemoryPropertyFlags       properties,
                                                       AllocType                   allocType,
                                                       const char* pDebugName ) const
{
    VkDeviceMemory       memory;

    VkMemoryAllocateInfo memAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = physDevice->GetMemoryTypeIndex( memReqs.memoryTypeBits, properties ),
    };

    VkMemoryAllocateFlagsInfo allocFlagInfo = {};
    if( allocType == AllocType::WITH_ADDRESS_QUERY )
    {
        allocFlagInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        };
        memAllocInfo.pNext = &allocFlagInfo;
    }

    VkResult r = vkAllocateMemory( device, &memAllocInfo, nullptr, &memory );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, memory, VK_OBJECT_TYPE_DEVICE_MEMORY, pDebugName );
    return memory;
}

VkDeviceMemory RTGL1::MemoryAllocator::AllocDedicated( const VkMemoryRequirements2& memReqs2,
                                                       VkMemoryPropertyFlags        properties,
                                                       AllocType                    allocType,
                                                       const char* pDebugName ) const
{
    return AllocDedicated( memReqs2.memoryRequirements, properties, allocType, pDebugName );
}

void RTGL1::MemoryAllocator::FreeDedicated( VkDevice device, VkDeviceMemory memory )
{
    vkFreeMemory( device, memory, nullptr );
}
