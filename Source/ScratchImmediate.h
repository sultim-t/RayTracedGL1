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

#pragma once

#include <cassert>
#include <optional>
#include <span>
#include <vector>

#include "RTGL1/RTGL1.h"

namespace RTGL1
{

class ScratchImmediate
{
public:
    struct PrimitiveRange
    {
        uint32_t startVertex;
        uint32_t end;

        uint32_t Count() const
        {
            assert( startVertex <= end );
            return end - startVertex;
        }
    };

public:
    void Clear();
    void StartPrimitive( RgUtilImScratchTopology topology );
    void EndPrimitive();

    void Vertex( float x, float y, float z )
    {
        accumVertex.position[ 0 ] = x;
        accumVertex.position[ 1 ] = y;
        accumVertex.position[ 2 ] = z;

        if( accumTexLayer1 )
        {
            texLayer1.push_back( *accumTexLayer1 );
        }
        if( accumTexLayer2 )
        {
            texLayer2.push_back( *accumTexLayer2 );
        }
        if( accumTexLayerLightmap )
        {
            texLayerLightmap.push_back( *accumTexLayerLightmap );
        }

        verts.push_back( accumVertex );
    }

    void Normal( float x, float y, float z )
    {
        accumVertex.normal[ 0 ] = x;
        accumVertex.normal[ 1 ] = y;
        accumVertex.normal[ 2 ] = z;
    }

    void TexCoord( float u, float v )
    {
        accumVertex.texCoord[ 0 ] = u;
        accumVertex.texCoord[ 1 ] = v;
    }
    void TexCoord_Layer1( float u, float v ) { accumTexLayer1 = { u, v }; }
    void TexCoord_Layer2( float u, float v ) { accumTexLayer2 = { u, v }; }
    void TexCoord_LayerLightmap( float u, float v ) { accumTexLayerLightmap = { u, v }; }

    void Color( RgColor4DPacked32 color ) { accumVertex.color = color; }

    void SetToPrimitive( RgMeshPrimitiveInfo* pTarget );

    std::span< const uint32_t > GetIndices( RgUtilImScratchTopology topology,
                                            uint32_t                vertexCount );

private:
    std::vector< RgPrimitiveVertex > verts;
    std::vector< RgFloat2D >         texLayer1;
    std::vector< RgFloat2D >         texLayer2;
    std::vector< RgFloat2D >         texLayerLightmap;
    std::optional< PrimitiveRange >  lastbatch;

    std::vector< uint32_t > indicesTriangles;
    std::vector< uint32_t > indicesTriangleStrip;
    std::vector< uint32_t > indicesTriangleFan;
    std::vector< uint32_t > indicesQuad;

    std::vector< uint32_t > accumIndices;

    RgPrimitiveVertex accumVertex{
        .position = { 0.0f, 0.0f, 0.0f },
        .normal   = { 0.0f, 1.0f, 0.0f },
        .tangent  = { 0.0f, 0.0f, 1.0f },
        .texCoord = { 0.0f, 0.0f },
        .color    = rgUtilPackColorByte4D( 255, 255, 255, 255 ),
    };
    std::optional< RgUtilImScratchTopology > accumTopology;
    std::optional< RgFloat2D >               accumTexLayer1;
    std::optional< RgFloat2D >               accumTexLayer2;
    std::optional< RgFloat2D >               accumTexLayerLightmap;
};

}
