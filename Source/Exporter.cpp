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

#include "Exporter.h"

#define CGLTF_VALIDATE_ENABLE_ASSERTS 1
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf/cgltf_write.h"

#include <cassert>
#include <fstream>

RTGL1::Exporter::Exporter()
{

}

RTGL1::Exporter::~Exporter()
{
    // TODO: check if ExportToFiles was called
    assert(1);
}

void RTGL1::Exporter::AddPrimitive( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive )
{
}

namespace 
{
bool is_cstr_empty(const char *cstr)
{
    return cstr == nullptr || *cstr == '\0';
}

// clang-format off
#define RG_TRANSFORM_TO_GLTF_MATRIX( t )                                        \
    {                                                                           \
        ( t ).matrix[ 0 ][ 0 ], ( t ).matrix[ 0 ][ 1 ], ( t ).matrix[ 0 ][ 2 ], \
        ( t ).matrix[ 0 ][ 3 ], ( t ).matrix[ 1 ][ 0 ], ( t ).matrix[ 1 ][ 1 ], \
        ( t ).matrix[ 1 ][ 2 ], ( t ).matrix[ 1 ][ 3 ], ( t ).matrix[ 2 ][ 0 ], \
        ( t ).matrix[ 2 ][ 1 ], ( t ).matrix[ 2 ][ 2 ], ( t ).matrix[ 2 ][ 3 ], \
        0.f, 0.f, 0.f, 1.f,                                                     \
    }
// clang-format on

}

void RTGL1::Exporter::ExportToFiles( const std::filesystem::path& folder )
{
    const RgPrimitiveVertex verts[] = {
        {
            .position = { -1, 0, -1 },
            .normal   = { 0, 1, 0 },
            .tangent  = { 1, 0, 0 },
            .texCoord = { 0, 0 },
            .color    = rgUtilPackColorByte4D( 255, 0, 0, 255 ),
        },
        {
            .position = { 5, 0, -1 },
            .normal   = { 0, 1, 0 },
            .tangent  = { 1, 0, 0 },
            .texCoord = { 0, 0 },
            .color    = rgUtilPackColorByte4D( 255, 0, 0, 255 ),
        },
        {
            .position = { -1, 0, -1 },
            .normal   = { 0, 1, 0 },
            .tangent  = { 1, 0, 0 },
            .texCoord = { 0, 0 },
            .color    = rgUtilPackColorByte4D( 255, 0, 0, 255 ),
        },
    };

    const RgMeshInfo rgmesh = {
        .pMeshName = "Triangle",
    };

    const RgMeshPrimitiveInfo rgprimitive = {
        .pPrimitiveNameInMesh  = "Prim",
        .primitiveIndexInMesh = 0,
        .transform            = { {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
        } },
        .pVertices            = verts,
        .vertexCount          = std::size( verts ),
        .pIndices             = nullptr,
        .indexCount           = 0,
        .pTextureName         = nullptr,
        .textureFrame         = 0,
        .color                = rgUtilPackColorByte4D( 255, 255, 255, 255 ),
        .emissive             = 0,
        .pEditorInfo          = nullptr,
    };

    if( is_cstr_empty( rgmesh.pMeshName ) )
    {
        assert( 0 );
        return;
    }

    const void* binData = rgprimitive.pVertices;
    const auto  binSize = rgprimitive.vertexCount * sizeof( *rgprimitive.pVertices );
    const auto  binUri  = rgmesh.pMeshName + std::string( ".bin" );

    const auto outGltf = folder / ( rgmesh.pMeshName + std::string( ".gltf" ) );
    const auto outBin  = folder / binUri;


    const char* primExtrasExample = nullptr /*"{\n"
                                    "    \"portalOutPosition\" : [0,0,0]\n"
                                    "}\n"*/;

    const char* sceneExtrasExample = nullptr /*"{\n"
                                     "    \"tonemapping_enable\" : 1\n"
                                     "}\n"*/;


    // Note: const_cast, assuming that cgltf_write_file makes only read-only access
    
    cgltf_data data = {
        .asset =
            cgltf_asset{
                .copyright   = nullptr,
                .generator   = const_cast< char* >( "RTGL1" ),
                .version     = const_cast< char* >( "2.0" ),
                .min_version = nullptr,
            },
    };

    cgltf_buffer buffers[] = {
        {
            .name = nullptr,
            .size = binSize,
            .uri  = const_cast< char* >( binUri.c_str() ),
        },
    };
    data.buffers       = std::data( buffers );
    data.buffers_count = std::size( buffers );

    cgltf_buffer_view bufferViews[] = {
        {
            .name   = nullptr,
            .buffer = &buffers[ 0 ],
            .offset = 0,
            .size   = binSize,
            .stride = sizeof( RgPrimitiveVertex ),
            .type   = cgltf_buffer_view_type_vertices,
        },
    };
    data.buffer_views       = std::data( bufferViews );
    data.buffer_views_count = std::size( bufferViews );

    cgltf_accessor accessors[] = {
        {
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = false,
            .type           = cgltf_type_vec3,
            .offset         = offsetof( RgPrimitiveVertex, position ),
            .count          = rgprimitive.vertexCount,
            .buffer_view    = &bufferViews[ 0 ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
        {
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = true,
            .type           = cgltf_type_vec3,
            .offset         = offsetof( RgPrimitiveVertex, normal ),
            .count          = rgprimitive.vertexCount,
            .buffer_view    = &bufferViews[ 0 ],
            .has_min        = true,
            .min            = { -1.f, -1.f, -1.f },
            .has_max        = true,
            .max            = { 1.f, 1.f, 1.f },
        },
        {
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = true,
            .type           = cgltf_type_vec4,
            .offset         = offsetof( RgPrimitiveVertex, tangent ),
            .count          = rgprimitive.vertexCount,
            .buffer_view    = &bufferViews[ 0 ],
            .has_min        = true,
            .min            = { -1.f, -1.f, -1.f, -1.f },
            .has_max        = true,
            .max            = { 1.f, 1.f, 1.f, 1.f },
        },
        {
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = false,
            .type           = cgltf_type_vec2,
            .offset         = offsetof( RgPrimitiveVertex, texCoord ),
            .count          = rgprimitive.vertexCount,
            .buffer_view    = &bufferViews[ 0 ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
        {
            .name           = nullptr,
            .component_type = cgltf_component_type_r_8u,
            .normalized     = false,
            .type           = cgltf_type_vec4,
            .offset         = offsetof( RgPrimitiveVertex,color ),
            .count          = rgprimitive.vertexCount,
            .buffer_view    = &bufferViews[ 0 ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
    };
    data.accessors       = std::data( accessors );
    data.accessors_count = std::size( accessors );


    cgltf_attribute attributes[] = {
        {
            .name  = const_cast< char* >( "POSITION" ),
            .type  = cgltf_attribute_type_position,
            .index = 0,
            .data  = &accessors[ 0 ],
        },
        {
            .name  = const_cast< char* >( "NORMAL" ),
            .type  = cgltf_attribute_type_normal,
            .index = 0,
            .data  = &accessors[ 1 ],
        },
        {
            .name  = const_cast< char* >( "TANGENT" ),
            .type  = cgltf_attribute_type_tangent,
            .index = 0,
            .data  = &accessors[ 2 ],
        },
        {
            .name  = const_cast< char* >( "TEXCOORD_0" ),
            .type  = cgltf_attribute_type_texcoord,
            .index = 0,
            .data  = &accessors[ 3 ],
        },
        {
            .name  = const_cast< char* >( "COLOR" ),
            .type  = cgltf_attribute_type_color,
            .index = 0,
            .data  = &accessors[ 4 ],
        },
    };


    cgltf_primitive primitive = {
        .type             = cgltf_primitive_type_triangles,
        .indices          = nullptr,
        .material         = nullptr,
        .attributes       = attributes,
        .attributes_count = std::size( attributes ),
        .extras           = {},
    };

    cgltf_mesh meshes[] = {
        {
            .name             = const_cast< char* >( rgprimitive.pPrimitiveNameInMesh ),
            .primitives       = &primitive,
            .primitives_count = 1,
            .extras           = {},
        },
    };
    data.meshes       = std::data( meshes );
    data.meshes_count = std::size( meshes );


    std::vector< cgltf_node > nodes;
    nodes.reserve( 1 + data.meshes_count );
    // RgMesh node
    nodes.emplace_back( cgltf_node{
        .name           = const_cast< char* >( rgmesh.pMeshName ),
        .parent         = nullptr,
        .children       = nullptr, // later
        .children_count = 0,       // later
        .extras         = { .data = nullptr },
    } );
    // RgMeshPrimitive nodes
    for( const auto& m : meshes )
    {
        nodes.emplace_back( cgltf_node{
            .name                    = const_cast< char* >( m.name ),
            .parent                  = nullptr, // later
            .children                = nullptr,
            .children_count          = 0,
            .mesh                    = &meshes[ 0 ],
            .camera                  = nullptr,
            .light                   = nullptr,
            .has_matrix              = true,
            .matrix                  = RG_TRANSFORM_TO_GLTF_MATRIX( rgprimitive.transform ),
            .extras                  = { .data = const_cast< char* >( primExtrasExample ) },
            .has_mesh_gpu_instancing = false,
            .mesh_gpu_instancing     = {},
        } );
    }
    // link, as pointers are stable at this point
    std::vector< cgltf_node* > mainNodeChildren; 
    auto* mainNode = &nodes[ 0 ];
    {
        for( auto& n : nodes )
        {
            if( &n != mainNode )
            {
                mainNodeChildren.push_back( &n );
            }
        }

        mainNode->children       = mainNodeChildren.data();
        mainNode->children_count = mainNodeChildren.size();
        for( auto* n : mainNodeChildren )
        {
            n->parent = mainNode;
        }
    }
    data.nodes       = std::data( nodes );
    data.nodes_count = std::size( nodes );

    
    cgltf_scene scenes[] = {
        {
            .name        = const_cast< char* >( "default" ),
            .nodes       = &mainNode,
            .nodes_count = 1,
            .extras      = { .data = const_cast< char* >( sceneExtrasExample ) },
        },
    };
    data.scenes       = std::data( scenes );
    data.scenes_count = std::size( scenes );
    data.scene        = &scenes[ 0 ];

    cgltf_options options = {};
    cgltf_result  r;

    r = cgltf_validate( &data );
    if( r != cgltf_result_success)
    {
        assert( 0 );
        return;
    }

    r = cgltf_write_file( &options, outGltf.string().c_str(), &data );
    if( r != cgltf_result_success )
    {
        assert( 0 );
        return;
    }

    {
        std::ofstream fbin( outBin, std::ios::out | std::ios::trunc | std::ios::binary );
        fbin.write( static_cast< const char* >( binData ), std::streamsize( binSize ) );
    }
}
