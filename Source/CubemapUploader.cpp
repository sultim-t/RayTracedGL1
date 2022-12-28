// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "CubemapUploader.h"

RTGL1::TextureUploader::UploadResult RTGL1::CubemapUploader::UploadImage( const UploadInfo& info )
{
    assert( info.isCubemap );
    // cubemaps can't be updateable
    assert( !info.isUpdateable );

    constexpr uint32_t FaceCount = 6;

    VkImage            image;
    VkBuffer           stagingBuffers[ FaceCount ] = {};
    void*              mappedData[ FaceCount ]     = {};


    // allocate and fill buffer
    const auto         faceSize = VkDeviceSize( info.dataSize );

    VkBufferCreateInfo stagingInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = faceSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    for( uint32_t i = 0; i < FaceCount; i++ )
    {
        stagingBuffers[ i ] = memAllocator->CreateStagingSrcTextureBuffer(
            &stagingInfo, info.pDebugName, &mappedData[ i ] );

        // if couldn't allocate memory
        if( stagingBuffers[ i ] == VK_NULL_HANDLE )
        {
            // clear allocated
            for( uint32_t j = 0; j < i; j++ )
            {
                memAllocator->DestroyStagingSrcTextureBuffer( stagingBuffers[ j ] );
            }

            return UploadResult{};
        }

        SET_DEBUG_NAME( device, stagingBuffers[ i ], VK_OBJECT_TYPE_BUFFER, info.pDebugName );
    }


    bool wasCreated = CreateImage( info, &image );
    if( !wasCreated )
    {
        // clean created resources
        for( VkBuffer b : stagingBuffers )
        {
            memAllocator->DestroyStagingSrcTextureBuffer( b );
        }

        return UploadResult{};
    }


    // copy image data to buffer
    for( uint32_t i = 0; i < FaceCount; i++ )
    {
        memcpy( mappedData[ i ], info.cubemap.pFaces[ i ], faceSize );
    }


    // and copy it to image
    PrepareImage( image, stagingBuffers, info, ImagePrepareType::INIT );


    // create image view
    VkImageView imageView = CreateImageView( image,
                                             info.format,
                                             info.isCubemap,
                                             GetMipmapCount( info.baseSize, info ),
                                             std::nullopt );
    SET_DEBUG_NAME( device, imageView, VK_OBJECT_TYPE_IMAGE_VIEW, info.pDebugName );


    // push staging buffer to be deleted when it won't be in use
    for( VkBuffer b : stagingBuffers )
    {
        stagingToFree[ info.frameIndex ].push_back( b );
    }


    // return results

    return UploadResult{
        .wasUploaded = true,
        .image       = image,
        .view        = imageView,
    };
}
