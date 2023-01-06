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

#include "Buffer.h"

using namespace RTGL1;

Buffer::Buffer()
    : device( VK_NULL_HANDLE )
    , buffer( VK_NULL_HANDLE )
    , memory( VK_NULL_HANDLE )
    , address( 0 )
    , size( 0 )
    , isMapped( false )
{
}

Buffer::~Buffer()
{
    Destroy();
}

void Buffer::Init( MemoryAllocator&      allocator,
                   VkDeviceSize          bsize,
                   VkBufferUsageFlags    usage,
                   VkMemoryPropertyFlags properties,
                   const char*           debugName )
{
    if( bsize == 0 )
    {
        assert( 0 );
        return;
    }

    device = allocator.GetDevice();

    VkResult r;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size               = bsize;
    bufferInfo.usage              = usage;
    bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    r = vkCreateBuffer( device, &bufferInfo, nullptr, &buffer );
    VK_CHECKERROR( r );

    VkMemoryRequirements memReq = {};
    vkGetBufferMemoryRequirements( device, buffer, &memReq );

    memory = allocator.AllocDedicated(
        memReq, properties, MemoryAllocator::AllocType::WITH_ADDRESS_QUERY, debugName );

    r = vkBindBufferMemory( device, buffer, memory, 0 );
    VK_CHECKERROR( r );

    if( bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT )
    {
        VkBufferDeviceAddressInfoKHR addrInfo = {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer,
        };
        address = vkGetBufferDeviceAddress( device, &addrInfo );
    }

    SET_DEBUG_NAME( device, buffer, VK_OBJECT_TYPE_BUFFER, debugName );

    size = bsize;
}

void Buffer::Destroy()
{
    assert( !isMapped );

    if( device == VK_NULL_HANDLE )
    {
        return;
    }

    // memory is guaranteed to be dedicated
    if( memory != VK_NULL_HANDLE )
    {
        vkFreeMemory( device, memory, nullptr );
        memory = VK_NULL_HANDLE;
    }

    if( buffer != VK_NULL_HANDLE )
    {
        vkDestroyBuffer( device, buffer, nullptr );
        buffer = VK_NULL_HANDLE;
    }

    address = 0;
    size    = 0;
}

void* Buffer::Map()
{
    assert( device != VK_NULL_HANDLE );
    assert( !isMapped );
    assert( memory != VK_NULL_HANDLE && size > 0 );

    isMapped = true;
    void* mapped;

    VkResult r = vkMapMemory( device, memory, 0, size, 0, &mapped );
    VK_CHECKERROR( r );

    return mapped;
}

void Buffer::Unmap()
{
    assert( device != VK_NULL_HANDLE );
    assert( isMapped );
    isMapped = false;
    vkUnmapMemory( device, memory );
}

bool Buffer::TryUnmap()
{
    assert( device != VK_NULL_HANDLE );

    if( isMapped )
    {
        Unmap();
        return true;
    }

    return false;
}

VkBuffer Buffer::GetBuffer() const
{
    assert( buffer != VK_NULL_HANDLE );
    return buffer;
}

VkDeviceMemory Buffer::GetMemory() const
{
    assert( memory != VK_NULL_HANDLE );
    return memory;
}

VkDeviceAddress Buffer::GetAddress() const
{
    assert( address != 0 );
    return address;
}

VkDeviceSize Buffer::GetSize() const
{
    assert( ( buffer != VK_NULL_HANDLE && size != 0 ) ||
            ( buffer == VK_NULL_HANDLE && size == 0 ) );
    return size;
}

bool Buffer::IsMapped() const
{
    return isMapped;
}

bool Buffer::IsInitted() const
{
    return buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE;
}
