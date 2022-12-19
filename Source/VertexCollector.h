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

#include <optional>
#include <span>
#include <vector>

#include "Buffer.h"
#include "Common.h"
#include "Material.h"
#include "VertexCollectorFilter.h"
#include "RTGL1/RTGL1.h"

namespace RTGL1
{

struct ShGeometryInstance;
struct ShVertex;

class GeomInfoManager;

// The class collects vertex data to buffers with shader struct types.
// Geometries are passed to the class by chunks and the result of collecting
// is a vertex buffer with ready data and infos for acceleration structure creation/building.
class VertexCollector
{
public:
    explicit VertexCollector( VkDevice                                  device,
                              const std::shared_ptr< MemoryAllocator >& allocator,
                              std::shared_ptr< GeomInfoManager >        geomInfoManager,
                              VkDeviceSize                              bufferSize,
                              VertexCollectorFilterTypeFlags            filters );

    // Create new vertex collector, but with shared device local buffers
    explicit VertexCollector( const std::shared_ptr< const VertexCollector >& src,
                              const std::shared_ptr< MemoryAllocator >&       allocator );

    ~VertexCollector();

    VertexCollector( const VertexCollector& other )                = delete;
    VertexCollector( VertexCollector&& other ) noexcept            = delete;
    VertexCollector& operator=( const VertexCollector& other )     = delete;
    VertexCollector& operator=( VertexCollector&& other ) noexcept = delete;

    
    bool AddPrimitive( uint32_t                          frameIndex,
                       const RgMeshInfo&                 parentMesh,
                       const RgMeshPrimitiveInfo&        info,
                       std::span< MaterialTextures, 4 >  layerTextures,
                       std::span< RgColor4DPacked32, 4 > layerColors );


    // Clear data that was generated while collecting.
    // Should be called when blasGeometries is not needed anymore
    void Reset();
    // Copy buffer from staging and set barrier for processing in compute shader
    // "isStaticVertexData" is required to determine what GLSL struct to use for copying
    bool CopyFromStaging( VkCommandBuffer cmd );


    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;
    uint32_t GetCurrentVertexCount() const;
    uint32_t GetCurrentIndexCount() const;


    // Get primitive counts from filters. Null if corresponding filter wasn't found.
    const std::vector< uint32_t >& GetPrimitiveCounts(
        VertexCollectorFilterTypeFlags filter ) const;

    // Get AS geometries data from filters. Null if corresponding filter wasn't found.
    const std::vector< VkAccelerationStructureGeometryKHR >& GetASGeometries(
        VertexCollectorFilterTypeFlags filter ) const;

    // Get AS build range infos from filters. Null if corresponding filter wasn't found.
    const std::vector< VkAccelerationStructureBuildRangeInfoKHR >& GetASBuildRangeInfos(
        VertexCollectorFilterTypeFlags filter ) const;


    // Are all geometries for each filter type in "flags" empty?
    bool AreGeometriesEmpty( VertexCollectorFilterTypeFlags flags ) const;
    // Are all geometries of this type empty?
    bool AreGeometriesEmpty( VertexCollectorFilterTypeFlagBits type ) const;


    // Make sure that copying was done
    void InsertVertexPreprocessBeginBarrier( VkCommandBuffer cmd );
    // Make sure that preprocessing is done, and prepare for use in AS build and in shaders
    void InsertVertexPreprocessFinishBarrier( VkCommandBuffer cmd );

private:
    void InitStagingBuffers( const std::shared_ptr< MemoryAllocator >& allocator );

    void CopyDataToStaging( const RgMeshPrimitiveInfo& info, uint32_t vertIndex );

    bool CopyVertexDataFromStaging( VkCommandBuffer cmd );
    bool CopyIndexDataFromStaging( VkCommandBuffer cmd );
    bool CopyTransformsFromStaging( VkCommandBuffer cmd, bool insertMemBarrier );

    // Parse flags to flag bit pairs and create instances of
    // VertexCollectorFilter. Flag bit pair contains one bit from
    // each flag bit group (e.g. change frequency group and pass through group).
    void InitFilters( VertexCollectorFilterTypeFlags flags );

    void     AddFilter( VertexCollectorFilterTypeFlags filterGroup );
    uint32_t PushGeometry( VertexCollectorFilterTypeFlags            type,
                           const VkAccelerationStructureGeometryKHR& geom );
    void     PushPrimitiveCount( VertexCollectorFilterTypeFlags type, uint32_t primCount );
    void     PushRangeInfo( VertexCollectorFilterTypeFlags                  type,
                            const VkAccelerationStructureBuildRangeInfoKHR& rangeInfo );

    uint32_t GetGeometryCount( VertexCollectorFilterTypeFlags type );
    uint32_t GetAllGeometryCount() const;

private:
    VkDevice                       device;
    VertexCollectorFilterTypeFlags filtersFlags;

    Buffer                    stagingVertBuffer;
    std::shared_ptr< Buffer > vertBuffer;

    Buffer                    stagingIndexBuffer;
    std::shared_ptr< Buffer > indexBuffer;

    Buffer                    stagingTransformsBuffer;
    std::shared_ptr< Buffer > transformsBuffer;

    std::shared_ptr< GeomInfoManager > geomInfoMgr;

    uint32_t curVertexCount;
    uint32_t curIndexCount;
    uint32_t curPrimitiveCount;
    uint32_t curTransformCount;

    ShVertex*             mappedVertexData;
    uint32_t*             mappedIndexData;
    VkTransformMatrixKHR* mappedTransformData;

    rgl::unordered_map< VertexCollectorFilterTypeFlags, std::shared_ptr< VertexCollectorFilter > >
        filters;
};

}