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

#include "GltfImporter.h"

#include "Const.h"
#include "Matrix.h"

#include "cgltf/cgltf.h"

#include <format>

namespace
{
    
RgTransform ColumnsToRows( const float arr[ 16 ] )
{
#define MAT( i, j ) arr[ ( i )*4 + ( j ) ]

    assert( std::abs( MAT( 0, 3 ) ) < FLT_EPSILON );
    assert( std::abs( MAT( 1, 3 ) ) < FLT_EPSILON );
    assert( std::abs( MAT( 2, 3 ) ) < FLT_EPSILON );
    assert( std::abs( MAT( 2, 3 ) - 1.0f ) < FLT_EPSILON );

    return RgTransform{ {
        { MAT( 0, 0 ), MAT( 1, 0 ), MAT( 2, 0 ), MAT( 3, 0 ) },
        { MAT( 0, 1 ), MAT( 1, 1 ), MAT( 2, 1 ), MAT( 3, 1 ) },
        { MAT( 0, 2 ), MAT( 1, 2 ), MAT( 2, 2 ), MAT( 3, 2 ) },
    } };

#undef MAT
}

RgTransform MakeRgTransformFromGltfNode( const cgltf_node& node )
{
    float mat[ 16 ];
    cgltf_node_transform_local( &node, mat );

    return ColumnsToRows( mat );
}

void ApplyInverseWorldTransform( cgltf_node &mainNode, const RgTransform& worldTransform )
{
    mainNode.has_translation = false;
    mainNode.has_rotation    = false;
    mainNode.has_scale       = false;
    memset( mainNode.translation, 0, sizeof( mainNode.translation ) );
    memset( mainNode.rotation, 0, sizeof( mainNode.rotation ) );
    memset( mainNode.scale, 0, sizeof( mainNode.scale ) );

    mainNode.has_matrix = true;

    // columns
    const float gltfMatrixWorld[] = RG_TRANSFORM_TO_GLTF_MATRIX( worldTransform );

    float inv[ 16 ];
    RTGL1::Matrix::Inverse( inv, gltfMatrixWorld );

    float original[ 16 ];
    cgltf_node_transform_local( &mainNode, original );

    RTGL1::Matrix::Multiply( mainNode.matrix, inv, original );
}

cgltf_node* FindMainRootNode( cgltf_data* data )
{
    if( !data || !data->scene )
    {
        return nullptr;
    }

    std::span ns( data->scene->nodes, data->scene->nodes_count );

    for( cgltf_node* n : ns )
    {
        if( RTGL1::Utils::IsCstrEmpty( n->name ) )
        {
            continue;
        }

        if( std::strcmp( n->name, RTGL1_MAIN_ROOT_NODE ) == 0 )
        {
            return n;
        }
    }

    return nullptr;
}
}

std::vector< RgPrimitiveVertex > RTGL1::GltfImporter::GatherVertices( const cgltf_node& node )
{
    assert( node.mesh );
    assert( node.parent );

    /*if( node.mesh->primitives_count < 1 )
    {
        debugprint(
            std::format(
                "{}: No primitives on (...->{}->{}). Ignoring", gltfPath, node.parent->name, node.name )
                .c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return {};
    }
    else if( node.mesh->primitives_count > 2 )
    {
        debugprint( std::format( "{}: No primitives on (...->{}->{}). Ignoring",
                                 gltfPath,
                                 node.parent->name,
                                 node.name )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return {};
    }*/

    for( size_t i = 0; i < node.mesh->primitives_count; i++ )
    {
        const cgltf_primitive& prim = node.mesh->primitives[ i ];

        for( size_t a = 0; a < prim.attributes_count; a++ )
        {
            const cgltf_attribute& attr = prim.attributes[ a ];

            switch( attr.type )
            {
                case cgltf_attribute_type_position: break;
                case cgltf_attribute_type_normal: break;
                case cgltf_attribute_type_tangent: break;
                case cgltf_attribute_type_texcoord: {
                    int texcoordIndex = attr.index;

                    break;
                }
                case cgltf_attribute_type_color: break;
                default: break;
            }



        }
    }

    return {};
}



RTGL1::GltfImporter::GltfImporter( const std::filesystem::path& _gltfPath,
                                   const RgTransform&           _worldTransform,
                                   DebugPrintFn                 _debugprint )
    : data( nullptr ), gltfPath( _gltfPath.string() ), debugprint( std::move( _debugprint ) )
{
    cgltf_result  r{ cgltf_result_success };
    cgltf_options options{};
    cgltf_data*   parsedData{ nullptr };

    struct FreeIfFail
    {
        cgltf_data** data;
        cgltf_data** parsedData;
        ~FreeIfFail()
        {
            if( *data == nullptr )
            {
                cgltf_free( *parsedData );
            }
        }
    } tmp = { &data, &parsedData };

    r = cgltf_parse_file( &options, gltfPath.c_str(), &parsedData );
    if( r != cgltf_result_success )
    {
        debugprint(
            std::format( "{}: {}. Error code: {}", gltfPath, "cgltf_parse_file", int( r ) ).c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    r = cgltf_load_buffers( &options, parsedData, gltfPath.c_str() );
    if( r != cgltf_result_success )
    {
        debugprint(
            std::format( "{}: {}. Error code: {}", gltfPath, "cgltf_load_buffers", int( r ) )
                .c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    r = cgltf_validate( parsedData );
    if( r != cgltf_result_success )
    {
        debugprint(
            std::format( "{}: {}. Error code: {}", gltfPath, "cgltf_validate", int( r ) ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    if( parsedData->scenes_count == 0 )
    {
        debugprint( std::format( "{}: {}", gltfPath, "No scenes found" ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    if( parsedData->scene == nullptr )
    {
        debugprint( std::format( "{}: {}", gltfPath, "No default scene, using first" ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        parsedData->scene = &parsedData->scenes[ 0 ];
    }

    cgltf_node* mainNode = FindMainRootNode( parsedData );

    if( !mainNode )
    {
        debugprint(
            std::format( "{}: {}", gltfPath, "No \"" RTGL1_MAIN_ROOT_NODE "\" node in the default scene" )
                .c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    ApplyInverseWorldTransform( *mainNode, _worldTransform );

    data = parsedData;
}

RTGL1::GltfImporter::~GltfImporter()
{
    cgltf_free( data );
}

void RTGL1::GltfImporter::UploadAsStaticToScene( Scene& scene )
{
    cgltf_node* mainNode = FindMainRootNode( data );
    if( !mainNode )
    {
        return;
    }

    if( mainNode->mesh )
    {
        debugprint( std::format( "{}: Found a mesh attached to node ({}). Main node should can't "
                                 "have meshes. Ignoring",
                                 gltfPath,
                                 mainNode->name )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
    }

    std::span srcMeshes( mainNode->children, mainNode->children_count );
    for( cgltf_node* srcMesh : srcMeshes )
    {
        if( Utils::IsCstrEmpty( srcMesh->name ) )
        {
            debugprint(
                std::format( "{}: Found srcMesh with null name (a child node of {}). Ignoring",
                             gltfPath,
                             mainNode->name )
                    .c_str(),
                RG_MESSAGE_SEVERITY_WARNING );
            continue;
        }

        if( srcMesh->mesh )
        {
            debugprint( std::format( "{}: Found a mesh attached to node ({}->{}). Only child nodes "
                                     "of it can have meshes. Ignoring",
                                     gltfPath,
                                     mainNode->name,
                                     srcMesh->name )
                            .c_str(),
                        RG_MESSAGE_SEVERITY_WARNING );
        }

        // TODO: really bad way to reduce hash64 to 32 bits
        RgMeshInfo dstMesh = {
            .uniqueObjectID =
                uint32_t( std::hash< std::string_view >{}( srcMesh->name ) % UINT32_MAX ),
            .pMeshName    = srcMesh->name,
            .isExportable = false,
        };
        
        for( size_t i = 0; i < srcMesh->children_count; i++ )
        {
            cgltf_node* srcPrim = srcMesh->children[ i ];

            if( !srcPrim->mesh )
            {
                continue;
            }

            if( Utils::IsCstrEmpty( srcPrim->name ) )
            {
                debugprint(
                    std::format(
                        "{}: Found srcPrim with null name (a child node of {}->{}). Ignoring",
                        gltfPath,
                        mainNode->name,
                        srcMesh->name )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
                continue;
            }

            if( srcPrim->children_count != 0 )
            {
                debugprint(
                    std::format( "{}: Found child nodes of ({}->{}->{}). Ignoring",
                                 gltfPath,
                                 mainNode->name,
                                 srcMesh->name,
                                 srcPrim->name )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
            }
            
            RgMeshPrimitiveInfo dstPrim = {
                .pPrimitiveNameInMesh = srcPrim->name,
                .primitiveIndexInMesh = uint32_t( i ),
                .flags                = 0,
                .transform            = MakeRgTransformFromGltfNode( *srcPrim ),
               /* .pVertices            =,
                .vertexCount          =,
                .pIndices             =,
                .indexCount =
                    , .pTextureName =, .textureFrame =, .color =, .emissive =, .pEditorInfo =.,*/
            };

        }
    }

}


