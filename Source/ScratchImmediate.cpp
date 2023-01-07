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

#include "ScratchImmediate.h"

#include <cassert>

#include "RgException.h"

namespace
{

uint32_t GetTriangleCount( RgUtilImScratchTopology topology, uint32_t vertexCount )
{
    switch( topology )
    {
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLES: {
            return vertexCount / 3u;
        }
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_STRIP:
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_FAN: {
            return std ::max( 0u, vertexCount - 2 );
        }
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_QUADS: {
            uint32_t quadsCount = vertexCount / 4;
            return quadsCount * 2;
        }
        default: return 0;
    }
}

uint32_t GetIndexCount( RgUtilImScratchTopology topology, uint32_t vertexCount )
{
    return GetTriangleCount( topology, vertexCount ) * 3;
}

constexpr uint32_t ALLOC_STEP = 600;

uint32_t GetNextAllocStep( uint32_t old )
{
    uint32_t m = ( old + ALLOC_STEP ) / ALLOC_STEP;
    return m * ALLOC_STEP;
}

template< bool CounterClockwise >
const uint32_t* GetIndicesTriangles( std::vector< uint32_t >& existing, uint32_t required )
{
    const auto curIndexCount = uint32_t( existing.size() );

    assert( curIndexCount % 3 == 0 );
    assert( required % 3 == 0 );
    assert( ALLOC_STEP % 3 == 0 );

    if( required > curIndexCount )
    {
        required = GetNextAllocStep( required );
        assert( required % 3 == 0 );

        uint32_t startTri    = curIndexCount / 3;
        uint32_t newTriCount = required / 3;

        existing.resize( required );

        for( uint32_t tri = startTri; tri < newTriCount; tri++ )
        {
            existing[ tri * 3 + ( CounterClockwise ? 2 : 0 ) ] = tri * 3 + 0;
            existing[ tri * 3 + ( CounterClockwise ? 1 : 1 ) ] = tri * 3 + 1;
            existing[ tri * 3 + ( CounterClockwise ? 0 : 2 ) ] = tri * 3 + 2;
        }
    }

    return existing.data();
}

template< bool CounterClockwise >
const uint32_t* GetIndicesTriangleStrip( std::vector< uint32_t >& existing, uint32_t required )
{
    const auto curIndexCount = uint32_t( existing.size() );

    assert( curIndexCount % 3 == 0 );
    assert( required % 3 == 0 );
    assert( ALLOC_STEP % 3 == 0 );

    if( required > curIndexCount )
    {
        required = GetNextAllocStep( required );
        assert( required % 3 == 0 );

        uint32_t startTri    = curIndexCount / 3;
        uint32_t newTriCount = required / 3;

        existing.resize( required );

        for( uint32_t tri = startTri; tri < newTriCount; tri++ )
        {
            existing[ tri * 3 + ( CounterClockwise ? 2 : 0 ) ] = tri;
            existing[ tri * 3 + ( CounterClockwise ? 1 : 1 ) ] = tri + ( 1 + tri % 2 );
            existing[ tri * 3 + ( CounterClockwise ? 0 : 2 ) ] = tri + ( 2 - tri % 2 );
        }
    }

    return existing.data();
}

template< bool CounterClockwise >
const uint32_t* GetIndicesTriangleFan( std::vector< uint32_t >& existing, uint32_t required )
{
    const auto curIndexCount = uint32_t( existing.size() );

    assert( curIndexCount % 3 == 0 );
    assert( required % 3 == 0 );
    assert( ALLOC_STEP % 3 == 0 );

    if( required > curIndexCount )
    {
        required = GetNextAllocStep( required );
        assert( required % 3 == 0 );

        uint32_t startTri    = curIndexCount / 3;
        uint32_t newTriCount = required / 3;

        existing.resize( required );

        for( uint32_t tri = startTri; tri < newTriCount; tri++ )
        {
            existing[ tri * 3 + ( CounterClockwise ? 2 : 0 ) ] = tri + 1;
            existing[ tri * 3 + ( CounterClockwise ? 1 : 1 ) ] = tri + 2;
            existing[ tri * 3 + ( CounterClockwise ? 0 : 2 ) ] = 0;
        }
    }

    return existing.data();
}

template< bool CounterClockwise >
const uint32_t* GetIndicesQuads( std::vector< uint32_t >& existing, uint32_t required )
{
    const auto curIndexCount = uint32_t( existing.size() );

    assert( curIndexCount % 6 == 0 );
    assert( required % 6 == 0 );
    assert( ALLOC_STEP % 6 == 0 );

    if( required > curIndexCount )
    {
        required = GetNextAllocStep( required );
        assert( required % 6 == 0 );

        uint32_t startQuad    = curIndexCount / 6;
        uint32_t newQuadCount = required / 6;

        existing.resize( required );

        for( uint32_t quad = startQuad; quad < newQuadCount; quad++ )
        {
            existing[ quad * 6 + ( CounterClockwise ? 2 : 0 ) ] = quad * 4 + 0;
            existing[ quad * 6 + ( CounterClockwise ? 1 : 1 ) ] = quad * 4 + 1;
            existing[ quad * 6 + ( CounterClockwise ? 0 : 2 ) ] = quad * 4 + 2;

            existing[ quad * 6 + ( CounterClockwise ? 5 : 3 ) ] = quad * 4 + 2;
            existing[ quad * 6 + ( CounterClockwise ? 4 : 4 ) ] = quad * 4 + 3;
            existing[ quad * 6 + ( CounterClockwise ? 3 : 5 ) ] = quad * 4 + 0;
        }
    }

    return existing.data();
}

}


std::span< const uint32_t > RTGL1::ScratchImmediate::GetIndices( RgUtilImScratchTopology topology,
                                                                 uint32_t vertexCount )
{
    uint32_t indexCount = GetIndexCount( topology, vertexCount );

    switch( topology )
    {
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLES:
            return {
                GetIndicesTriangles< true >( indicesTriangles, indexCount ),
                indexCount,
            };
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_STRIP:
            return {
                GetIndicesTriangleStrip< true >( indicesTriangleStrip, indexCount ),
                indexCount,
            };
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_FAN:
            return {
                GetIndicesTriangleFan< true >( indicesTriangleFan, indexCount ),
                indexCount,
            };
        case RG_UTIL_IM_SCRATCH_TOPOLOGY_QUADS:
            return {
                GetIndicesQuads< true >( indicesQuad, indexCount ),
                indexCount,
            };
        default:
            throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                               "ScratchGetIndices: Unknown topology" );
    }
}

void RTGL1::ScratchImmediate::Clear()
{
    verts.clear();
    texLayer1.clear();
    texLayer2.clear();
    texLayerLightmap.clear();
    lastbatch = std::nullopt;
    accumIndices.clear();
    accumTopology         = std::nullopt;
    accumTexLayer1        = std::nullopt;
    accumTexLayer2        = std::nullopt;
    accumTexLayerLightmap = std::nullopt;
}

void RTGL1::ScratchImmediate::StartPrimitive( RgUtilImScratchTopology topology )
{
    lastbatch     = PrimitiveRange{ .startVertex = uint32_t( verts.size() ), .end = 0 };
    accumTopology = topology;
}

void RTGL1::ScratchImmediate::EndPrimitive()
{
    if( !lastbatch || !accumTopology )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_CALL,
                           "Corresponding ImScratchStart was not called for rgUtilImScratchEnd" );
    }

    lastbatch->end = uint32_t( verts.size() );
    assert( lastbatch->startVertex <= lastbatch->end );

    auto localIndices = GetIndices( *accumTopology, lastbatch->Count() );
    for( const auto i : localIndices )
    {
        accumIndices.push_back( lastbatch->startVertex + i );
    }

    lastbatch = std::nullopt;
}

namespace
{

void TrySetTexLayer( RgMeshPrimitiveInfo* pTarget, uint32_t layerIndex, std::span< RgFloat2D > src )
{
    if( src.empty() )
    {
        return;
    }

    // each texcoord is tied to each vertex, so amount must be the same
    if( src.size() != pTarget->vertexCount )
    {
        assert( 0 );
        return;
    }

    // nowhere to set
    if( !pTarget || !pTarget->pEditorInfo )
    {
        assert( 0 );
        return;
    }

    const RgEditorTextureLayerInfo* dst = nullptr;

    switch( layerIndex )
    {
        case 1: dst = &pTarget->pEditorInfo->layer1; break;
        case 2: dst = &pTarget->pEditorInfo->layer2; break;
        case 3: dst = &pTarget->pEditorInfo->layerLightmap; break;
        default: assert( 0 ); return;
    }

    // nowhere to set
    if( !dst )
    {
        assert( 0 );
        return;
    }

    const_cast< RgEditorTextureLayerInfo* >( dst )->pTexCoord = src.data();
}

}

void RTGL1::ScratchImmediate::SetToPrimitive( RgMeshPrimitiveInfo* pTarget )
{
    if( !pTarget )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "pTarget is null" );
    }

    pTarget->pVertices   = verts.data();
    pTarget->vertexCount = uint32_t( verts.size() );

    pTarget->pIndices   = accumIndices.data();
    pTarget->indexCount = uint32_t( accumIndices.size() );

    TrySetTexLayer( pTarget, 1, texLayer1 );
    TrySetTexLayer( pTarget, 2, texLayer2 );
    TrySetTexLayer( pTarget, 3, texLayerLightmap );
}
