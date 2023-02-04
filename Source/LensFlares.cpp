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

#include "LensFlares.h"

#include "Utils.h"

#include "Generated/ShaderCommonC.h"

namespace
{
constexpr bool LENSFLARES_IN_WORLDSPACE = true;


constexpr VkDeviceSize MAX_VERTEX_COUNT = 1 << 16;
constexpr VkDeviceSize MAX_INDEX_COUNT  = 1 << 18;


// indirectDrawCommands: one uint32_t - for count, the rest - cmds
constexpr VkDeviceSize GetIndirectDrawCommandsOffset()
{
    return 0;
}
constexpr VkDeviceSize GetIndirectDrawCountOffset()
{
    return LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof( RTGL1::ShIndirectDrawCommand );
}
constexpr RTGL1::ShIndirectDrawCommand* GetIndirectDrawCommandsArrayStart(
    void* pCullingInputBuffer )
{
    return ( RTGL1::ShIndirectDrawCommand* )( ( void* )( ( uint8_t* )pCullingInputBuffer +
                                                         GetIndirectDrawCommandsOffset() ) );
}


static_assert( offsetof( RTGL1::ShIndirectDrawCommand, indexCount ) ==
                   offsetof( VkDrawIndexedIndirectCommand, indexCount ),
               "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand" );
static_assert( offsetof( RTGL1::ShIndirectDrawCommand, instanceCount ) ==
                   offsetof( VkDrawIndexedIndirectCommand, instanceCount ),
               "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand" );
static_assert( offsetof( RTGL1::ShIndirectDrawCommand, firstIndex ) ==
                   offsetof( VkDrawIndexedIndirectCommand, firstIndex ),
               "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand" );
static_assert( offsetof( RTGL1::ShIndirectDrawCommand, vertexOffset ) ==
                   offsetof( VkDrawIndexedIndirectCommand, vertexOffset ),
               "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand" );
static_assert( offsetof( RTGL1::ShIndirectDrawCommand, firstInstance ) ==
                   offsetof( VkDrawIndexedIndirectCommand, firstInstance ),
               "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand" );
}



RTGL1::LensFlares::LensFlares( VkDevice                            _device,
                               std::shared_ptr< MemoryAllocator >& _allocator,
                               const ShaderManager&                _shaderManager,
                               VkRenderPass                        _renderPass,
                               const GlobalUniform&                _uniform,
                               const Framebuffers&                 _framebuffers,
                               const TextureManager&               _textureManager,
                               const RgInstanceCreateInfo&         _instanceInfo )
    : device( _device )
    , cullingInputCount( 0 )
    , vertexCount( 0 )
    , indexCount( 0 )
    , vertFragPipelineLayout( VK_NULL_HANDLE )
    , rasterDescPool( VK_NULL_HANDLE )
    , rasterDescSet( VK_NULL_HANDLE )
    , rasterDescSetLayout( VK_NULL_HANDLE )
    , cullPipelineLayout( VK_NULL_HANDLE )
    , cullPipeline( VK_NULL_HANDLE )
    , cullDescPool( VK_NULL_HANDLE )
    , cullDescSet( VK_NULL_HANDLE )
    , cullDescSetLayout( VK_NULL_HANDLE )
    , isPointToCheckInScreenSpace( !LENSFLARES_IN_WORLDSPACE )
{
    cullingInput   = std::make_unique< AutoBuffer >( _allocator );
    vertexBuffer   = std::make_unique< AutoBuffer >( _allocator );
    indexBuffer    = std::make_unique< AutoBuffer >( _allocator );
    instanceBuffer = std::make_unique< AutoBuffer >( _allocator );


    cullingInput->Create( LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof( ShIndirectDrawCommand ),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          "Lens flares culling input" );

    indirectDrawCommands.Init(
        *_allocator,
        LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof( ShIndirectDrawCommand ) + sizeof( uint32_t ),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Lens flares draw cmds" );

    vertexBuffer->Create( MAX_VERTEX_COUNT * sizeof( ShVertex ),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          "Lens flares vertex buffer" );

    indexBuffer->Create( MAX_INDEX_COUNT * sizeof( uint32_t ),
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         "Lens flares index buffer" );

    instanceBuffer->Create( LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof( ShLensFlareInstance ),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            "Lens flares instance buffer" );


    CreateCullDescriptors();
    CreateRasterDescriptors();


    CreatePipelineLayouts( _uniform.GetDescSetLayout(),
                           _textureManager.GetDescSetLayout(),
                           rasterDescSetLayout,
                           cullDescSetLayout,
                           _framebuffers.GetDescSetLayout() );


    rasterPipelines =
        std::make_unique< RasterizerPipelines >( device,
                                                 vertFragPipelineLayout,
                                                 _renderPass,
                                                 _shaderManager,
                                                 "VertLensFlare",
                                                 "FragLensFlare",
                                                 1 /* emission, for compatibility */,
                                                 _instanceInfo.rasterizedVertexColorGamma );

    CreatePipelines( &_shaderManager );
}

RTGL1::LensFlares::~LensFlares()
{
    vkDestroyDescriptorPool( device, rasterDescPool, nullptr );
    vkDestroyDescriptorSetLayout( device, rasterDescSetLayout, nullptr );
    vkDestroyDescriptorPool( device, cullDescPool, nullptr );
    vkDestroyDescriptorSetLayout( device, cullDescSetLayout, nullptr );

    vkDestroyPipelineLayout( device, vertFragPipelineLayout, nullptr );
    vkDestroyPipelineLayout( device, cullPipelineLayout, nullptr );

    DestroyPipelines();
}


void RTGL1::LensFlares::PrepareForFrame( uint32_t frameIndex )
{
    cullingInputCount = 0;
    vertexCount       = 0;
    indexCount        = 0;
}

void RTGL1::LensFlares::Upload( uint32_t                     frameIndex,
                                const RgLensFlareUploadInfo& uploadInfo,
                                float                        emissiveMult,
                                const TextureManager&        textureManager )
{
    if( cullingInputCount + 1 >= LENS_FLARES_MAX_DRAW_CMD_COUNT )
    {
        debug::Warning( "Too many lens flares. Limit: {}", LENS_FLARES_MAX_DRAW_CMD_COUNT );
        return;
    }
    if( vertexCount + uploadInfo.vertexCount >= MAX_VERTEX_COUNT )
    {
        debug::Warning( "Too many lens flare vertices. Limit: {}", MAX_VERTEX_COUNT );
        return;
    }
    if( indexCount + uploadInfo.indexCount >= MAX_INDEX_COUNT )
    {
        debug::Warning( "Too many lens flare indices. Limit: {}", MAX_INDEX_COUNT );
        return;
    }


    const uint32_t instanceIndex = cullingInputCount;
    const uint32_t vertexIndex   = vertexCount;
    const uint32_t indexIndex    = indexCount;
    cullingInputCount++;
    vertexCount += uploadInfo.vertexCount;
    indexCount += uploadInfo.indexCount;


    // vertices
    {
        // must be same to copy
        static_assert(
            std::is_same_v< decltype( uploadInfo.pVertices ), const RgPrimitiveVertex* > );
        static_assert( sizeof( ShVertex ) == sizeof( RgPrimitiveVertex ) );
        static_assert( offsetof( ShVertex, position ) == offsetof( RgPrimitiveVertex, position ) );
        static_assert( offsetof( ShVertex, normal ) == offsetof( RgPrimitiveVertex, normal ) );
        static_assert( offsetof( ShVertex, texCoord ) == offsetof( RgPrimitiveVertex, texCoord ) );
        static_assert( offsetof( ShVertex, color ) == offsetof( RgPrimitiveVertex, color ) );

        auto* dst = vertexBuffer->GetMappedAs< ShVertex* >( frameIndex );
        memcpy( &dst[ vertexIndex ],
                uploadInfo.pVertices,
                uploadInfo.vertexCount * sizeof( ShVertex ) );
    }


    // indices
    {
        auto* dst = indexBuffer->GetMappedAs< uint32_t* >( frameIndex );
        memcpy(
            &dst[ indexIndex ], uploadInfo.pIndices, uploadInfo.indexCount * sizeof( uint32_t ) );
    }


    // instances
    auto tex = textureManager.GetMaterialTextures( uploadInfo.pTextureName );

    ShLensFlareInstance instance = {
        .packedColor          = Utils::PackColor( 255, 255, 255, 255 ),
        .textureIndex         = tex.indices[ TEXTURE_ALBEDO_ALPHA_INDEX ],
        .emissiveTextureIndex = tex.indices[ TEXTURE_EMISSIVE_INDEX ],
        .emissiveMult         = emissiveMult,
    };

    {
        auto* dst = instanceBuffer->GetMappedAs< ShLensFlareInstance* >( frameIndex );
        memcpy( &dst[ instanceIndex ], &instance, sizeof( ShLensFlareInstance ) );
    }


    // draw cmds
    ShIndirectDrawCommand input = {
        .indexCount        = uploadInfo.indexCount,
        .instanceCount     = 1,
        .firstIndex        = indexIndex,
        .vertexOffset      = int32_t( vertexIndex ),
        .firstInstance     = instanceIndex, // to access instance buffer with gl_InstanceIndex
        .positionToCheck_X = uploadInfo.pointToCheck.data[ 0 ],
        .positionToCheck_Y = uploadInfo.pointToCheck.data[ 1 ],
        .positionToCheck_Z = uploadInfo.pointToCheck.data[ 2 ],
    };

    {
        ShIndirectDrawCommand* dst =
            GetIndirectDrawCommandsArrayStart( cullingInput->GetMapped( frameIndex ) );
        memcpy( &dst[ instanceIndex ], &input, sizeof( ShIndirectDrawCommand ) );
    }
}

void RTGL1::LensFlares::SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    if( cullingInputCount == 0 || vertexCount == 0 || indexCount == 0 )
    {
        return;
    }

    cullingInput->CopyFromStaging(
        cmd, frameIndex, cullingInputCount * sizeof( ShIndirectDrawCommand ) );
    vertexBuffer->CopyFromStaging( cmd, frameIndex, vertexCount * sizeof( ShVertex ) );
    indexBuffer->CopyFromStaging( cmd, frameIndex, indexCount * sizeof( uint32_t ) );
    instanceBuffer->CopyFromStaging(
        cmd, frameIndex, cullingInputCount * sizeof( ShLensFlareInstance ) );
}

void RTGL1::LensFlares::Cull( VkCommandBuffer      cmd,
                              uint32_t             frameIndex,
                              const GlobalUniform& uniform,
                              const Framebuffers&  framebuffers )
{
    if( cullingInputCount == 0 )
    {
        return;
    }

    // sync
    {
        VkBufferMemoryBarrier2KHR bs[] = {
            {
                .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                .buffer        = cullingInput->GetDeviceLocal(),
                .offset        = 0,
                .size          = cullingInputCount * sizeof( ShIndirectDrawCommand ),
            },
        };

        VkDependencyInfoKHR info = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .bufferMemoryBarrierCount = std::size( bs ),
            .pBufferMemoryBarriers    = bs,
        };

        svkCmdPipelineBarrier2KHR( cmd, &info );
    }


    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipeline );

    VkDescriptorSet sets[] = {
        uniform.GetDescSet( frameIndex ),
        framebuffers.GetDescSet( frameIndex ),
        cullDescSet,
    };
    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_COMPUTE,
                             cullPipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    uint32_t inputCount = cullingInputCount;
    vkCmdPushConstants( cmd,
                        cullPipelineLayout,
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        0,
                        sizeof( inputCount ),
                        &inputCount );

    uint32_t wgCount =
        Utils::GetWorkGroupCount( cullingInputCount, COMPUTE_INDIRECT_DRAW_FLARES_GROUP_SIZE_X );
    vkCmdDispatch( cmd, wgCount, 1, 1 );
}

void RTGL1::LensFlares::SyncForDraw( VkCommandBuffer cmd, uint32_t frameIndex )
{
    if( cullingInputCount == 0 )
    {
        return;
    }

    VkBufferMemoryBarrier2KHR bs[] = {
        {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,
            .buffer        = indirectDrawCommands.GetBuffer(),
            .offset        = GetIndirectDrawCommandsOffset(),
            .size          = cullingInputCount * sizeof( ShIndirectDrawCommand ),
        },
        {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,
            .buffer        = indirectDrawCommands.GetBuffer(),
            .offset        = GetIndirectDrawCountOffset(),
            .size          = sizeof( uint32_t ),
        },
        {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR,
            .buffer        = instanceBuffer->GetDeviceLocal(),
            .offset        = 0,
            .size          = cullingInputCount * sizeof( ShLensFlareInstance ),
        },
        {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR,
            .buffer        = vertexBuffer->GetDeviceLocal(),
            .offset        = 0,
            .size          = vertexCount * sizeof( ShVertex ),
        },
        {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT_KHR,
            .buffer        = indexBuffer->GetDeviceLocal(),
            .offset        = 0,
            .size          = indexCount * sizeof( uint32_t ),
        },
    };

    VkDependencyInfoKHR info = {
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .bufferMemoryBarrierCount = std::size( bs ),
        .pBufferMemoryBarriers    = bs,
    };

    svkCmdPipelineBarrier2KHR( cmd, &info );
}

void RTGL1::LensFlares::Draw( VkCommandBuffer       cmd,
                              uint32_t              frameIndex,
                              const TextureManager& textureManager,
                              const float*          defaultViewProj )
{
    if( cullingInputCount == 0 )
    {
        return;
    }

    rasterPipelines->BindPipelineIfNew(
        cmd, VK_NULL_HANDLE, PipelineStateFlagBits::TRANSLUCENT | PipelineStateFlagBits::ADDITIVE );

    VkDescriptorSet sets[] = {
        textureManager.GetDescSet( frameIndex ),
        rasterDescSet,
    };
    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             rasterPipelines->GetPipelineLayout(),
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    static constexpr float Identity[ 4 ][ 4 ] = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 },
    };

    vkCmdPushConstants( cmd,
                        rasterPipelines->GetPipelineLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        16 * sizeof( float ),
                        LENSFLARES_IN_WORLDSPACE ? defaultViewProj
                                                 : reinterpret_cast< const float* >( Identity ) );

    VkBuffer     vb     = vertexBuffer->GetDeviceLocal();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers( cmd, 0, 1, &vb, &offset );
    vkCmdBindIndexBuffer( cmd, indexBuffer->GetDeviceLocal(), 0, VK_INDEX_TYPE_UINT32 );

    vkCmdDrawIndexedIndirectCount( cmd,
                                   indirectDrawCommands.GetBuffer(),
                                   GetIndirectDrawCommandsOffset(),
                                   indirectDrawCommands.GetBuffer(),
                                   GetIndirectDrawCountOffset(),
                                   LENS_FLARES_MAX_DRAW_CMD_COUNT,
                                   sizeof( ShIndirectDrawCommand ) );
}

uint32_t RTGL1::LensFlares::GetCullingInputCount() const
{
    return cullingInputCount;
}

void RTGL1::LensFlares::CreatePipelineLayouts( VkDescriptorSetLayout uniform,
                                               VkDescriptorSetLayout textures,
                                               VkDescriptorSetLayout raster,
                                               VkDescriptorSetLayout lensFlaresCull,
                                               VkDescriptorSetLayout framebufs )
{
    {
        VkDescriptorSetLayout s[] = {
            textures,
            raster,
        };

        VkPushConstantRange push = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset     = 0,
            .size       = 16 * sizeof( float ),
        };

        VkPipelineLayoutCreateInfo layoutInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = std::size( s ),
            .pSetLayouts            = s,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push,
        };

        VkResult r =
            vkCreatePipelineLayout( device, &layoutInfo, nullptr, &vertFragPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        vertFragPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Lens flares vert-frag pipeline layout" );
    }
    {
        VkDescriptorSetLayout s[] = {
            uniform,
            framebufs,
            lensFlaresCull,
        };

        VkPushConstantRange push = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = sizeof( uint32_t ),
        };

        VkPipelineLayoutCreateInfo layoutInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = std::size( s ),
            .pSetLayouts            = s,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push,
        };

        VkResult r = vkCreatePipelineLayout( device, &layoutInfo, nullptr, &cullPipelineLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        cullPipelineLayout,
                        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                        "Lens flares cull pipeline layout" );
    }
}

void RTGL1::LensFlares::CreatePipelines( const ShaderManager* shaderManager )
{
    VkSpecializationMapEntry entry = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof( uint32_t ),
    };

    VkSpecializationInfo spec = {
        .mapEntryCount = 1,
        .pMapEntries   = &entry,
        .dataSize      = sizeof( uint32_t ),
        .pData         = &isPointToCheckInScreenSpace,
    };

    VkComputePipelineCreateInfo info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = shaderManager->GetStageInfo( "CCullLensFlares" ),
        .layout = cullPipelineLayout,
    };
    info.stage.pSpecializationInfo = &spec;

    VkResult r = vkCreateComputePipelines( device, nullptr, 1, &info, nullptr, &cullPipeline );
    VK_CHECKERROR( r );
}

void RTGL1::LensFlares::DestroyPipelines()
{
    if( cullPipeline != nullptr )
    {
        vkDestroyPipeline( device, cullPipeline, nullptr );
    }
}

void RTGL1::LensFlares::OnShaderReload( const ShaderManager* shaderManager )
{
    rasterPipelines->OnShaderReload( shaderManager );

    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::LensFlares::CreateCullDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 2,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = 1,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &cullDescPool );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, cullDescPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Lens flare cull desc pool" );
    }
    {
        VkDescriptorSetLayoutBinding binding[] = {
            {
                .binding         = BINDING_LENS_FLARES_CULLING_INPUT,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            {
                .binding         = BINDING_LENS_FLARES_DRAW_CMDS,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::size( binding ),
            .pBindings    = binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &info, nullptr, &cullDescSetLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        cullDescSetLayout,
                        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "Lens flare cull desc set layout" );
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = cullDescPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &cullDescSetLayout,
        };

        VkResult r = vkAllocateDescriptorSets( device, &allocInfo, &cullDescSet );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, cullDescSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Lens flare cull desc set" );
    }
    {
        VkDescriptorBufferInfo bufs[] = {
            {
                .buffer = cullingInput->GetDeviceLocal(),
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            },
            {
                .buffer = indirectDrawCommands.GetBuffer(),
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            },
        };

        VkWriteDescriptorSet writes[] = {
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = cullDescSet,
                .dstBinding      = BINDING_LENS_FLARES_CULLING_INPUT,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo     = &bufs[ BINDING_LENS_FLARES_CULLING_INPUT ],
            },
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = cullDescSet,
                .dstBinding      = BINDING_LENS_FLARES_DRAW_CMDS,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo     = &bufs[ BINDING_LENS_FLARES_DRAW_CMDS ],
            },
        };

        vkUpdateDescriptorSets( device, std::size( writes ), writes, 0, nullptr );
    }
}

void RTGL1::LensFlares::CreateRasterDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = 1,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &rasterDescPool );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, rasterDescPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Lens flare raster desc pool" );
    }
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding         = BINDING_DRAW_LENS_FLARES_INSTANCES,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &info, nullptr, &rasterDescSetLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device,
                        rasterDescSetLayout,
                        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "Lens flare raster desc set layout" );
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = rasterDescPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &rasterDescSetLayout,
        };

        VkResult r = vkAllocateDescriptorSets( device, &allocInfo, &rasterDescSet );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, rasterDescSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Lens flare raster desc set" );
    }
    {
        VkDescriptorBufferInfo b = {
            .buffer = instanceBuffer->GetDeviceLocal(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet w = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = rasterDescSet,
            .dstBinding      = BINDING_DRAW_LENS_FLARES_INSTANCES,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &b,
        };

        vkUpdateDescriptorSets( device, 1, &w, 0, nullptr );
    }
}
