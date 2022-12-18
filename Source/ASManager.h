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

#pragma once

#include "ASBuilder.h"
#include "CommandBufferManager.h"
#include "GlobalUniform.h"
#include "ScratchBuffer.h"
#include "TextureManager.h"
#include "VertexCollector.h"
#include "ASComponent.h"

namespace RTGL1
{

struct ShVertPreprocessing;

class ASManager
{
public:
    struct TLASPrepareResult
    {
        VkAccelerationStructureInstanceKHR instances[ 45 ];
        uint32_t                           instanceCount;
    };

public:
    ASManager( VkDevice                                device,
               const PhysicalDevice&                   physDevice,
               std::shared_ptr< MemoryAllocator >      allocator,
               std::shared_ptr< CommandBufferManager > cmdManager,
               std::shared_ptr< GeomInfoManager >      geomInfoManager );
    ~ASManager();

    ASManager( const ASManager& other )                = delete;
    ASManager( ASManager&& other ) noexcept            = delete;
    ASManager& operator=( const ASManager& other )     = delete;
    ASManager& operator=( ASManager&& other ) noexcept = delete;


    void BeginStaticGeometry();
    // Submitting static geometry to the building is a heavy operation
    // with waiting for it to complete.
    void SubmitStaticGeometry();
    // If all the added geometries must be removed, call this function before submitting
    void ResetStaticGeometry();


    void BeginDynamicGeometry( VkCommandBuffer cmd, uint32_t frameIndex );
    void SubmitDynamicGeometry( VkCommandBuffer cmd, uint32_t frameIndex );


    bool AddMeshPrimitive( uint32_t                   frameIndex,
                           const RgMeshInfo&          mesh,
                           const RgMeshPrimitiveInfo& primitive,
                           bool                       isStatic,
                           const TextureManager&      textureManager );


    // Prepare data for building TLAS.
    // Also fill uniform with current state.
    std::pair< TLASPrepareResult, ShVertPreprocessing > PrepareForBuildingTLAS(
        uint32_t         frameIndex,
        ShGlobalUniform& uniformData,
        uint32_t         uniformData_rayCullMaskWorld,
        bool             allowGeometryWithSkyFlag,
        bool             disableRTGeometry ) const;
    void BuildTLAS( VkCommandBuffer cmd, uint32_t frameIndex, const TLASPrepareResult& info );


    // Copy current dynamic vertex and index data to
    // special buffers for using current frame's data in the next frame.
    void CopyDynamicDataToPrevBuffers( VkCommandBuffer cmd, uint32_t frameIndex );


    void OnVertexPreprocessingBegin( VkCommandBuffer cmd, uint32_t frameIndex, bool onlyDynamic );
    void OnVertexPreprocessingFinish( VkCommandBuffer cmd, uint32_t frameIndex, bool onlyDynamic );


    VkDescriptorSet GetBuffersDescSet( uint32_t frameIndex ) const;
    VkDescriptorSet GetTLASDescSet( uint32_t frameIndex ) const;

    VkDescriptorSetLayout GetBuffersDescSetLayout() const;
    VkDescriptorSetLayout GetTLASDescSetLayout() const;

private:
    void CreateDescriptors();
    void UpdateBufferDescriptors( uint32_t frameIndex );
    void UpdateASDescriptors( uint32_t frameIndex );

    bool SetupBLAS( BLASComponent& as, const VertexCollector& vertCollector );

    void UpdateBLAS( BLASComponent& as, const VertexCollector& vertCollector );

    static bool SetupTLASInstanceFromBLAS( const BLASComponent& as,
                                           uint32_t             rayCullMaskWorld,
                                           bool                 allowGeometryWithSkyFlag,
                                           VkAccelerationStructureInstanceKHR& instance );

    static bool IsFastBuild( VertexCollectorFilterTypeFlags filter );

private:
    VkDevice                           device;
    std::shared_ptr< MemoryAllocator > allocator;

    VkFence staticCopyFence;

    // for filling buffers
    std::shared_ptr< VertexCollector > collectorStatic;
    std::shared_ptr< VertexCollector > collectorDynamic[ MAX_FRAMES_IN_FLIGHT ];
    // device-local buffer for storing previous info
    Buffer                             previousDynamicPositions;
    Buffer                             previousDynamicIndices;

    // building
    std::shared_ptr< ScratchBuffer > scratchBuffer;
    std::shared_ptr< ASBuilder >     asBuilder;

    std::shared_ptr< CommandBufferManager > cmdManager;
    std::shared_ptr< TextureManager >       textureMgr;
    std::shared_ptr< GeomInfoManager >      geomInfoMgr;

    std::vector< std::unique_ptr< BLASComponent > > allStaticBlas;
    std::vector< std::unique_ptr< BLASComponent > > allDynamicBlas[ MAX_FRAMES_IN_FLIGHT ];

    // top level AS
    std::unique_ptr< AutoBuffer >    instanceBuffer;
    std::unique_ptr< TLASComponent > tlas[ MAX_FRAMES_IN_FLIGHT ];

    // TLAS and buffer descriptors
    VkDescriptorPool descPool;

    VkDescriptorSetLayout buffersDescSetLayout;
    VkDescriptorSet       buffersDescSets[ MAX_FRAMES_IN_FLIGHT ];

    VkDescriptorSetLayout asDescSetLayout;
    VkDescriptorSet       asDescSets[ MAX_FRAMES_IN_FLIGHT ];
};

}