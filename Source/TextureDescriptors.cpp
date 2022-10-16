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

#include "TextureDescriptors.h"
#include "Const.h"

using namespace RTGL1;

TextureDescriptors::TextureDescriptors( VkDevice                          _device,
                                        std::shared_ptr< SamplerManager > _samplerManager,
                                        uint32_t                          _maxTextureCount,
                                        uint32_t                          _bindingIndex )
    : device( _device )
    , samplerManager( std::move( _samplerManager ) )
    , bindingIndex( _bindingIndex )
    , descPool( VK_NULL_HANDLE )
    , descLayout( VK_NULL_HANDLE )
    , descSets{}
    , emptyTextureImageView( VK_NULL_HANDLE )
    , emptyTextureImageLayout( VK_IMAGE_LAYOUT_UNDEFINED )
    , currentWriteCount( 0 )
{
    writeImageInfos.resize( _maxTextureCount );
    writeInfos.resize( _maxTextureCount );

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        writeCache[ i ].resize( _maxTextureCount );
    }

    CreateDescriptors( _maxTextureCount );
}

TextureDescriptors::~TextureDescriptors()
{
    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyDescriptorSetLayout( device, descLayout, nullptr );
}

VkDescriptorSet TextureDescriptors::GetDescSet( uint32_t frameIndex ) const
{
    return descSets[ frameIndex ];
}

VkDescriptorSetLayout TextureDescriptors::GetDescSetLayout() const
{
    return descLayout;
}

void TextureDescriptors::SetEmptyTextureInfo( VkImageView view )
{
    emptyTextureImageView   = view;
    emptyTextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void TextureDescriptors::CreateDescriptors( uint32_t maxTextureCount )
{
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding         = bindingIndex,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxTextureCount,
            .stageFlags      = VK_SHADER_STAGE_ALL,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &descLayout );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME(
            device, descLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Textures Desc set layout" );
    }

    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxTextureCount * MAX_FRAMES_IN_FLIGHT,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Textures Desc pool" );
    }

    {
        VkDescriptorSetAllocateInfo setInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descLayout,
        };
        for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
        {
            VkResult r = vkAllocateDescriptorSets( device, &setInfo, &descSets[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME(
                device, descSets[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET, "Textures desc set" );
        }
    }
}

bool TextureDescriptors::IsCached( uint32_t               frameIndex,
                                   uint32_t               textureIndex,
                                   VkImageView            view,
                                   SamplerManager::Handle samplerHandle )
{
    return writeCache[ frameIndex ][ textureIndex ].view == view &&
           writeCache[ frameIndex ][ textureIndex ].samplerHandle == samplerHandle;
}

void TextureDescriptors::AddToCache( uint32_t               frameIndex,
                                     uint32_t               textureIndex,
                                     VkImageView            view,
                                     SamplerManager::Handle samplerHandle )
{
    writeCache[ frameIndex ][ textureIndex ].view          = view;
    writeCache[ frameIndex ][ textureIndex ].samplerHandle = samplerHandle;
}

void TextureDescriptors::ResetCache( uint32_t frameIndex, uint32_t textureIndex )
{
    writeCache[ frameIndex ][ textureIndex ] = { VK_NULL_HANDLE, SamplerManager::Handle() };
}

void RTGL1::TextureDescriptors::ResetAllCache( uint32_t frameIndex )
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        for( auto& f : writeCache[ i ] )
        {
            f.view          = VK_NULL_HANDLE;
            f.samplerHandle = SamplerManager::Handle();
        }
    }
}

void TextureDescriptors::UpdateTextureDesc( uint32_t               frameIndex,
                                            uint32_t               textureIndex,
                                            VkImageView            view,
                                            SamplerManager::Handle samplerHandle )
{
    assert( view != VK_NULL_HANDLE );

    if( currentWriteCount >= writeInfos.size() )
    {
        assert( 0 );
        return;
    }

    // don't update if already is set to given parameters
    if( IsCached( frameIndex, textureIndex, view, samplerHandle ) )
    {
        return;
    }

    writeImageInfos[ currentWriteCount ] = VkDescriptorImageInfo{
        .sampler     = samplerManager->GetSampler( samplerHandle ),
        .imageView   = view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    writeInfos[ currentWriteCount ] = VkWriteDescriptorSet{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descSets[ frameIndex ],
        .dstBinding      = bindingIndex,
        .dstArrayElement = textureIndex,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &writeImageInfos[ currentWriteCount ],
    };
    currentWriteCount++;

    AddToCache( frameIndex, textureIndex, view, samplerHandle );
}

void TextureDescriptors::ResetTextureDesc( uint32_t frameIndex, uint32_t textureIndex )
{
    assert( emptyTextureImageView != VK_NULL_HANDLE &&
            emptyTextureImageLayout != VK_IMAGE_LAYOUT_UNDEFINED );

    // try to update with empty data
    UpdateTextureDesc( frameIndex,
                       textureIndex,
                       emptyTextureImageView,
                       SamplerManager::Handle( RG_SAMPLER_FILTER_NEAREST,
                                               RG_SAMPLER_ADDRESS_MODE_REPEAT,
                                               RG_SAMPLER_ADDRESS_MODE_REPEAT ) );
}

void TextureDescriptors::FlushDescWrites()
{
    vkUpdateDescriptorSets( device, currentWriteCount, writeInfos.data(), 0, nullptr );
    currentWriteCount = 0;
}
