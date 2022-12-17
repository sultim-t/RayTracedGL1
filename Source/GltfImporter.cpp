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
    assert( std::abs( MAT( 3, 3 ) - 1.0f ) < FLT_EPSILON );

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

void ApplyInverseWorldTransform( cgltf_node& mainNode, const RgTransform& worldTransform )
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

const char* NodeName( const cgltf_node& node )
{
    return node.name ? node.name : "";
}

const char* NodeName( const cgltf_node* node )
{
    return NodeName( *node );
}

template< size_t N >
cgltf_bool cgltf_accessor_read_float_h( const cgltf_accessor* accessor,
                                        cgltf_size            index,
                                        float ( &out )[ N ] )
{
    return cgltf_accessor_read_float( accessor, index, out, N );
}

}

std::vector< RgPrimitiveVertex > RTGL1::GltfImporter::GatherVertices( const cgltf_node& node )
{
    assert( node.mesh );
    assert( node.parent );

    if( node.mesh->primitives_count < 1 )
    {
        debugprint( std::format( "{}: Ignoring ...->{}->{}: No primitives on a mesh",
                                 gltfPath,
                                 NodeName( node.parent ),
                                 NodeName( node ) )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return {};
    }

    if( node.mesh->primitives_count > 2 )
    {
        debugprint( std::format( "{}: ...->{}->{}: Expected only 1 primitive on a mesh, but got "
                                 "{}. Parsing only the first",
                                 gltfPath,
                                 NodeName( node.parent ),
                                 NodeName( node ),
                                 node.mesh->primitives_count )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
    }

    const cgltf_primitive& prim = node.mesh->primitives[ 0 ];

    std::span attrSpan( prim.attributes, prim.attributes_count );

    auto debugprintAttr = [ this, &node ]( const cgltf_attribute& attr, std::string_view msg ) {
        debugprint( std::format( "{}: Ignoring ...->{}->{}: Attribute {}: {}",
                                 gltfPath,
                                 NodeName( node.parent ),
                                 NodeName( node ),
                                 attr.name ? attr.name : "",
                                 msg )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
    };

    // check if compatible and find common attribute count
    std::optional< size_t > vertexCount;
    {
        // required
        bool position{}, normal{}, tangent{}, texcoord{};

        for( const cgltf_attribute& attr : attrSpan )
        {
            if( attr.data->is_sparse )
            {
                debugprintAttr( attr, "Sparse accessors are not supported" );
                return {};
            }

            bool color = false;

            switch( attr.type )
            {
                case cgltf_attribute_type_position:
                    position = true;
                    if( cgltf_num_components( attr.data->type ) != 3 )
                    {
                        debugprintAttr( attr, "Expected VEC3" );
                        return {};
                    }
                    static_assert( std::size( RgPrimitiveVertex{}.position ) == 3 );
                    break;

                case cgltf_attribute_type_normal:
                    normal = true;
                    if( cgltf_num_components( attr.data->type ) != 3 )
                    {
                        debugprintAttr( attr, "Expected VEC3" );
                        return {};
                    }
                    static_assert( std::size( RgPrimitiveVertex{}.normal ) == 3 );
                    break;

                case cgltf_attribute_type_tangent:
                    tangent = true;
                    if( cgltf_num_components( attr.data->type ) != 4 )
                    {
                        debugprintAttr( attr, "Expected VEC4" );
                        return {};
                    }
                    static_assert( std::size( RgPrimitiveVertex{}.tangent ) == 4 );
                    break;

                case cgltf_attribute_type_texcoord:
                    texcoord = true;
                    if( cgltf_num_components( attr.data->type ) != 2 )
                    {
                        debugprintAttr( attr, "Expected VEC2" );
                        return {};
                    }
                    static_assert( std::size( RgPrimitiveVertex{}.texCoord ) == 2 );
                    break;


                case cgltf_attribute_type_color:
                    color = true;
                    if( cgltf_num_components( attr.data->type ) != 4 )
                    {
                        debugprintAttr( attr, "Expected VEC4" );
                        return {};
                    }
                    static_assert( std::is_same_v< decltype( RgPrimitiveVertex{}.color ),
                                                   RgColor4DPacked32 > );
                    break;

                default: break;
            }

            if( position || normal || tangent || texcoord || color )
            {
                if( vertexCount )
                {
                    if( vertexCount.value() != attr.data->count )
                    {
                        debugprintAttr(
                            attr,
                            std::format( "Mismatch on attributes count (expected {}, but got {})",
                                         *vertexCount,
                                         attr.data->count ) );
                        return {};
                    }
                }
                else
                {
                    vertexCount = attr.data->count;
                }
            }
        }

        if( !position || !normal || !tangent || !texcoord )
        {
            debugprint(
                std::format( "{}: Ignoring ...->{}->{}: Not all required attributes are present. "
                             "POSITION - {}. "
                             "NORMAL - {}. "
                             "TANGENT - {}. "
                             "TEXCOORD_0 - {}",
                             gltfPath,
                             NodeName( node.parent ),
                             NodeName( node ),
                             position,
                             normal,
                             tangent,
                             texcoord )
                    .c_str(),
                RG_MESSAGE_SEVERITY_WARNING );
            return {};
        }
    }

    if( !vertexCount )
    {
        debugprint(
            std::format(
                "{}: Ignoring ...->{}->{}: ", gltfPath, NodeName( node.parent ), NodeName( node ) )
                .c_str(),
            RG_MESSAGE_SEVERITY_VERBOSE );
        return {};
    }


    auto primVertices = std::vector< RgPrimitiveVertex >( *vertexCount );
    auto defaultColor = std::optional( rgUtilPackColorByte4D( 255, 255, 255, 255 ) );

    for( const cgltf_attribute& attr : attrSpan )
    {
        cgltf_bool ok = true;

        switch( attr.type )
        {
            case cgltf_attribute_type_position:
                for( size_t i = 0; i < primVertices.size(); i++ )
                {
                    ok &= cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].position );
                }
                break;

            case cgltf_attribute_type_normal:
                for( size_t i = 0; i < primVertices.size(); i++ )
                {
                    ok &= cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].normal );
                }
                break;

            case cgltf_attribute_type_tangent:
                for( size_t i = 0; i < primVertices.size(); i++ )
                {
                    ok &= cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].tangent );
                }
                break;

            case cgltf_attribute_type_texcoord: {
                int texcoordIndex = attr.index;
                for( size_t i = 0; i < primVertices.size(); i++ )
                {
                    ok &= cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].texCoord );
                }
                break;
            }

            case cgltf_attribute_type_color:
                defaultColor = std::nullopt;
                for( size_t i = 0; i < primVertices.size(); i++ )
                {
                    float c[ 4 ] = { 1, 1, 1, 1 };
                    ok &= cgltf_accessor_read_float_h( attr.data, i, c );
                    primVertices[ i ].color =
                        rgUtilPackColorFloat4D( c[ 0 ], c[ 1 ], c[ 2 ], c[ 3 ] );
                }
                break;

            default: break;
        }

        if( !ok )
        {
            debugprintAttr( attr, "cgltf_accessor_read_float fail" );
            return {};
        }
    }

    if( defaultColor )
    {
        for( auto& v : primVertices )
        {
            v.color = *defaultColor;
        }
    }

    return primVertices;
}

std::vector< uint32_t > RTGL1::GltfImporter::GatherIndices( const cgltf_node& node )
{
    // assuming that GatherVertices was already called,
    // so no need for extensive checks
    assert( node.mesh );
    assert( node.parent );
    assert( node.mesh->primitives_count > 0 );

    const cgltf_primitive& prim = node.mesh->primitives[ 0 ];

    if( prim.indices->is_sparse )
    {
        debugprint(
            std::format( "{}: Ignoring ...->{}->{}: Indices: Sparse accessors are not supported",
                         gltfPath,
                         NodeName( node.parent ),
                         NodeName( node ) )
                .c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return {};
    }

    std::vector< uint32_t > primIndices( prim.indices->count );

    for( size_t k = 0; k < prim.indices->count; k++ )
    {
        uint32_t resolved;

        if( !cgltf_accessor_read_uint( prim.indices, k, &resolved, 1 ) )
        {
            debugprint(
                std::format( "{}: Ignoring ...->{}->{}: Indices: cgltf_accessor_read_uint fail",
                             gltfPath,
                             node.parent->name,
                             node.name )
                    .c_str(),
                RG_MESSAGE_SEVERITY_WARNING );
            return {};
        }

        primIndices[ k ] = resolved;
    }

    return primIndices;
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
        debugprint( std::format( "{}: {}",
                                 gltfPath,
                                 "No \"" RTGL1_MAIN_ROOT_NODE "\" node in the default scene" )
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

void RTGL1::GltfImporter::UploadToScene_DEBUG( Scene& scene, uint32_t frameIndex )
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
                debugprint( std::format( "{}: Found child nodes of ({}->{}->{}). Ignoring",
                                         gltfPath,
                                         mainNode->name,
                                         srcMesh->name,
                                         srcPrim->name )
                                .c_str(),
                            RG_MESSAGE_SEVERITY_WARNING );
            }

            auto vertices = GatherVertices( *srcPrim );
            auto indices  = GatherIndices( *srcPrim );

            if( vertices.empty() )
            {
                continue;
            }

            RgMeshPrimitiveInfo dstPrim = {
                .pPrimitiveNameInMesh = srcPrim->name,
                .primitiveIndexInMesh = uint32_t( i ),
                .flags                = 0,
                .transform            = MakeRgTransformFromGltfNode( *srcPrim ),
                .pVertices            = vertices.data(),
                .vertexCount          = uint32_t( vertices.size() ),
                .pIndices             = indices.empty() ? nullptr : indices.data(),
                .indexCount           = uint32_t( indices.size() ),
                .pTextureName         = nullptr,
                .textureFrame         = 0,
                .color                = rgUtilPackColorByte4D( 255, 255, 255, 255 ),
                .emissive             = 0.0f,
                .pEditorInfo          = nullptr,
            };

            // TODO: static geom upload
            if( !scene.Upload( frameIndex, dstMesh, dstPrim ) )
            {
                assert( 0 );
            }
        }
    }
}
