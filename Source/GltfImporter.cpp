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
#include "Scene.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

#include "cgltf/cgltf.h"
#include <cfloat>

#include <format>

namespace RTGL1
{
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

    const char* CgltfErrorName( cgltf_result r )
    {
#define RTGL1_CGLTF_RESULT_NAME( x ) \
    case( x ): return "(" #x ")"; break

        switch( r )
        {
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_success );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_data_too_short );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_unknown_format );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_invalid_json );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_invalid_gltf );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_invalid_options );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_file_not_found );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_io_error );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_out_of_memory );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_legacy_gltf );
            RTGL1_CGLTF_RESULT_NAME( cgltf_result_max_enum );
            default: assert( 0 ); return "";
        }

#undef RTGL1_CGLTF_RESULT_NAME
    }

    template< size_t N >
    cgltf_bool cgltf_accessor_read_float_h( const cgltf_accessor* accessor,
                                            cgltf_size            index,
                                            float ( &out )[ N ] )
    {
        return cgltf_accessor_read_float( accessor, index, out, N );
    }

    std::vector< RgPrimitiveVertex > GatherVertices( const cgltf_primitive& prim,
                                                     const cgltf_node&      node,
                                                     std::string_view       gltfPath )
    {
        std::span attrSpan( prim.attributes, prim.attributes_count );

        auto debugprintAttr = [ &node, &gltfPath ]( const cgltf_attribute& attr,
                                                    std::string_view       msg ) {
            debug::Warning( "{}: Ignoring primitive of ...->{}->{}: Attribute {}: {}",
                            gltfPath,
                            NodeName( node.parent ),
                            NodeName( node ),
                            Utils::SafeCstr( attr.name ),
                            msg );
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
                                std::format(
                                    "Mismatch on attributes count (expected {}, but got {})",
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
                debug::Warning( "{}: Ignoring primitive of ...->{}->{}: Not all required "
                                "attributes are present. "
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
                                texcoord );
                return {};
            }
        }

        if( !vertexCount )
        {
            debug::Warning(
                "{}: Ignoring ...->{}->{}: ", gltfPath, NodeName( node.parent ), NodeName( node ) );
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
                        ok &=
                            cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].position );
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
                        ok &=
                            cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].tangent );
                    }
                    break;

                case cgltf_attribute_type_texcoord: {
                    int texcoordIndex = attr.index;
                    for( size_t i = 0; i < primVertices.size(); i++ )
                    {
                        ok &=
                            cgltf_accessor_read_float_h( attr.data, i, primVertices[ i ].texCoord );
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
                            Utils::PackColorFromFloat( c[ 0 ], c[ 1 ], c[ 2 ], c[ 3 ] );
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

    std::vector< uint32_t > GatherIndices( const cgltf_primitive& prim,
                                           const cgltf_node&      node,
                                           std::string_view       gltfPath )
    {
        if( prim.indices->is_sparse )
        {
            debug::Warning( "{}: Ignoring primitive of ...->{}->{}: Indices: Sparse accessors are "
                            "not supported",
                            gltfPath,
                            NodeName( node.parent ),
                            NodeName( node ) );
            return {};
        }

        std::vector< uint32_t > primIndices( prim.indices->count );

        for( size_t k = 0; k < prim.indices->count; k++ )
        {
            uint32_t resolved;

            if( !cgltf_accessor_read_uint( prim.indices, k, &resolved, 1 ) )
            {
                debug::Warning(
                    "{}: Ignoring primitive of ...->{}->{}: Indices: cgltf_accessor_read_uint fail",
                    gltfPath,
                    NodeName( node.parent ),
                    NodeName( node ) );
                return {};
            }

            primIndices[ k ] = resolved;
        }

        return primIndices;
    }

    std::string MakePTextureName( const cgltf_material&              mat,
                                  std::span< std::filesystem::path > fallbacks )
    {
        const cgltf_texture* t = mat.pbr_metallic_roughness.base_color_texture.texture;

        if( t && t->image && t->image->name )
        {
            std::string name = t->image->name;

            if( t->image->uri )
            {
                if( !std::string_view( t->image->uri )
                         .starts_with( TEXTURES_FOLDER_JUNCTION_PREFIX ) )
                {
                    debug::Warning( "Suspicious URI \"{}\" of an image with name \"{}\": "
                                    "If \"name\" field is provided, assumed that it's "
                                    "the original game texture. "
                                    "So expecting URI to start with {}. "
                                    "Texture overloading is disabled for this texture",
                                    t->image->uri,
                                    t->image->name,
                                    TEXTURES_FOLDER_JUNCTION_PREFIX );
                }
            }

            return name;
        }

        for( const auto& f : fallbacks )
        {
            if( !f.empty() )
            {
                return f.string();
            }
        }

        return "";
    }

    struct UploadTexturesResult
    {
        RgColor4DPacked32 color        = Utils::PackColor( 255, 255, 255, 255 );
        float             emissiveMult = 0.0f;
        std::string       pTextureName;
        float             metallicFactor  = 0.0f;
        float             roughnessFactor = 1.0f;
    };

    UploadTexturesResult UploadTextures( VkCommandBuffer              cmd,
                                         uint32_t                     frameIndex,
                                         const cgltf_material*        mat,
                                         TextureManager&              textureManager,
                                         const std::filesystem::path& gltfFolder,
                                         std::string_view             gltfPath )
    {
        if( mat == nullptr )
        {
            return {};
        }

        if( !mat->has_pbr_metallic_roughness )
        {
            debug::Warning( "{}: Ignoring material \"{}\":"
                            "Can't find PBR Metallic-Roughness",
                            gltfPath,
                            Utils::SafeCstr( mat->name ) );
            return {};
        }

        // clang-format off
        std::filesystem::path fullPaths[] = {
            std::filesystem::path(),
            std::filesystem::path(),
            std::filesystem::path(),
            std::filesystem::path(),
        };
        SamplerManager::Handle samplers[] = {
            SamplerManager::Handle( RG_SAMPLER_FILTER_AUTO, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            SamplerManager::Handle( RG_SAMPLER_FILTER_AUTO, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            SamplerManager::Handle( RG_SAMPLER_FILTER_AUTO, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            SamplerManager::Handle( RG_SAMPLER_FILTER_AUTO, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
        };
        static_assert( std::size( fullPaths ) == TEXTURES_PER_MATERIAL_COUNT );
        static_assert( std::size( samplers ) == TEXTURES_PER_MATERIAL_COUNT );
        // clang-format on


        const std::pair< int, const cgltf_texture_view& > txds[] = {
            {
                TEXTURE_ALBEDO_ALPHA_INDEX,
                mat->pbr_metallic_roughness.base_color_texture,
            },
            {
                TEXTURE_OCCLUSION_ROUGHNESS_METALLIC_INDEX,
                mat->pbr_metallic_roughness.metallic_roughness_texture,
            },
            {
                TEXTURE_NORMAL_INDEX,
                mat->normal_texture,
            },
            {
                TEXTURE_EMISSIVE_INDEX,
                mat->emissive_texture,
            },
        };
        static_assert( std::size( txds ) == TEXTURES_PER_MATERIAL_COUNT );

        RgTextureSwizzling pbrSwizzling = RG_TEXTURE_SWIZZLING_NULL_ROUGHNESS_METALLIC;
        {
            cgltf_texture* texRM = mat->pbr_metallic_roughness.metallic_roughness_texture.texture;
            cgltf_texture* texO  = mat->occlusion_texture.texture;

            if( texRM && texRM->image )
            {
                if( texO && texO->image )
                {
                    if( texRM->image == texO->image )
                    {
                        pbrSwizzling = RG_TEXTURE_SWIZZLING_OCCLUSION_ROUGHNESS_METALLIC;
                    }
                    else
                    {
                        debug::Warning( "{}: Ignoring occlusion image \"{}\" of material \"{}\": "
                                        "Occlusion should be in the Red channel of "
                                        "Metallic-Roughness image \"{}\"",
                                        gltfPath,
                                        Utils::SafeCstr( texO->image->uri ),
                                        Utils::SafeCstr( mat->name ),
                                        Utils::SafeCstr( texRM->image->uri ) );
                    }
                }
            }
            else
            {
                if( texO && texO->image )
                {
                    debug::Warning( "{}: Ignoring occlusion image \"{}\" of material \"{}\": "
                                    "Occlusion should be in the Red channel of Metallic-Roughness "
                                    "image which doesn't exist on this material",
                                    gltfPath,
                                    Utils::SafeCstr( texO->image->uri ),
                                    Utils::SafeCstr( mat->name ) );
                }
            }
        }


        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_sampler_magfilter
        constexpr auto makeRgSamplerFilter = []( int magFilter ) {
            return magFilter == 9728   ? RG_SAMPLER_FILTER_NEAREST
                   : magFilter == 9729 ? RG_SAMPLER_FILTER_LINEAR
                                       : RG_SAMPLER_FILTER_AUTO;
        };
        constexpr auto makeRgSamplerAddrMode = []( int wrap ) {
            return wrap == 33071 ? RG_SAMPLER_ADDRESS_MODE_CLAMP : RG_SAMPLER_ADDRESS_MODE_REPEAT;
        };

        for( const auto& [ index, txview ] : txds )
        {
            if( !txview.texture || !txview.texture->image )
            {
                continue;
            }

            if( txview.texcoord != 0 )
            {
                debug::Warning(
                    "{}: Ignoring texture {} of material \"{}\":"
                    "Only one layer of texture coordinates supported. Found TEXCOORD_{}",
                    gltfPath,
                    Utils::SafeCstr( txview.texture->name ),
                    Utils::SafeCstr( mat->name ),
                    txview.texcoord );
                continue;
            }

            if( Utils::IsCstrEmpty( txview.texture->image->uri ) )
            {
                debug::Warning( "{}: Ignoring texture {} of material \"{}\": "
                                "Texture's image URI is empty",
                                gltfPath,
                                Utils::SafeCstr( txview.texture->name ),
                                Utils::SafeCstr( mat->name ) );
                continue;
            }

            fullPaths[ index ] = gltfFolder / Utils::SafeCstr( txview.texture->image->uri );

            if( txview.texture->sampler )
            {
                samplers[ index ] = SamplerManager::Handle(
                    makeRgSamplerFilter( txview.texture->sampler->mag_filter ),
                    makeRgSamplerAddrMode( txview.texture->sampler->wrap_s ),
                    makeRgSamplerAddrMode( txview.texture->sampler->wrap_t ) );
            }
        }


        std::string materialName = MakePTextureName( *mat, fullPaths );

        // if fullPaths are empty
        if( !materialName.empty() )
        {
            textureManager.TryCreateImportedMaterial(
                cmd, frameIndex, materialName, fullPaths, samplers, pbrSwizzling );
        }

        if( auto t = mat->pbr_metallic_roughness.metallic_roughness_texture.texture )
        {
            if( t->image )
            {
                if( std::abs( mat->pbr_metallic_roughness.metallic_factor - 1.0f ) > 0.01f ||
                    std::abs( mat->pbr_metallic_roughness.roughness_factor - 1.0f ) > 0.01f )
                {
                    debug::Warning(
                        "{}: Texture with image \"{}\" of material \"{}\" has "
                        "metallic / roughness factors that are not 1.0. These values are "
                        "used by RTGL1 only if surface doesn't have PBR texture",
                        gltfPath,
                        Utils::SafeCstr( t->image->uri ),
                        Utils::SafeCstr( mat->name ) );
                }
            }
        }

        return UploadTexturesResult{
            .color = Utils::PackColorFromFloat( mat->pbr_metallic_roughness.base_color_factor ),
            .emissiveMult    = Utils::Luminance( mat->emissive_factor ),
            .pTextureName    = std::move( materialName ),
            .metallicFactor  = mat->pbr_metallic_roughness.metallic_factor,
            .roughnessFactor = mat->pbr_metallic_roughness.roughness_factor,
        };
    }

}
}

RTGL1::GltfImporter::GltfImporter( const std::filesystem::path& _gltfPath,
                                   const RgTransform&           _worldTransform,
                                   float                        _oneGameUnitInMeters )
    : data( nullptr )
    , gltfPath( _gltfPath.string() )
    , gltfFolder( _gltfPath.parent_path() )
    , oneGameUnitInMeters( _oneGameUnitInMeters )
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
    if( r == cgltf_result_file_not_found )
    {
        debug::Warning( "{}: Can't find a file, no static scene will be present", gltfPath );
        return;
    }
    else if( r != cgltf_result_success )
    {
        debug::Warning(
            "{}: cgltf_parse_file. Error code: {} {}", gltfPath, int( r ), CgltfErrorName( r ) );
        return;
    }

    r = cgltf_load_buffers( &options, parsedData, gltfPath.c_str() );
    if( r != cgltf_result_success )
    {
        debug::Warning(
            "{}: cgltf_load_buffers. Error code: {} {}. URI-s for .bin buffers might be incorrect",
            gltfPath,
            int( r ),
            CgltfErrorName( r ) );
        return;
    }

    r = cgltf_validate( parsedData );
    if( r != cgltf_result_success )
    {
        debug::Warning(
            "{}: cgltf_validate. Error code: {} {}", gltfPath, int( r ), CgltfErrorName( r ) );
        return;
    }

    if( parsedData->scenes_count == 0 )
    {
        debug::Warning( "{}: {}", gltfPath, "No scenes found" );
        return;
    }

    if( parsedData->scene == nullptr )
    {
        debug::Warning( "{}: {}", gltfPath, "No default scene, using first" );
        parsedData->scene = &parsedData->scenes[ 0 ];
    }

    cgltf_node* mainNode = FindMainRootNode( parsedData );

    if( !mainNode )
    {
        debug::Warning(
            "{}: {}", gltfPath, "No \"" RTGL1_MAIN_ROOT_NODE "\" node in the default scene" );
        return;
    }

    ApplyInverseWorldTransform( *mainNode, _worldTransform );

    data = parsedData;
}

RTGL1::GltfImporter::~GltfImporter()
{
    cgltf_free( data );
}

void RTGL1::GltfImporter::UploadToScene( VkCommandBuffer           cmd,
                                         uint32_t                  frameIndex,
                                         Scene&                    scene,
                                         TextureManager&           textureManager,
                                         const TextureMetaManager& textureMeta ) const
{
    cgltf_node* mainNode = FindMainRootNode( data );
    if( !mainNode )
    {
        return;
    }

    if( mainNode->mesh || mainNode->light )
    {
        debug::Warning( "{}: Main node ({}) should not have meshes / lights. Ignoring",
                        gltfPath,
                        mainNode->name );
    }

    // meshes
    for( cgltf_node* srcNode : std::span( mainNode->children, mainNode->children_count ) )
    {
        if( !srcNode || !srcNode->mesh )
        {
            continue;
        }

        if( Utils::IsCstrEmpty( srcNode->name ) )
        {
            debug::Warning( "{}: Found srcMesh with null name (a child node of {}). Ignoring",
                            gltfPath,
                            mainNode->name );
            continue;
        }

        if( srcNode->children_count > 0 )
        {
            debug::Warning( "{}: Found a child nodes of {}->{}. Ignoring them",
                            gltfPath,
                            mainNode->name,
                            srcNode->name );
        }
        
        auto primitiveExtra = json_parser::ReadStringAs< PrimitiveExtraInfo >(
            Utils::SafeCstr( srcNode->extras.data ) );

        // TODO: really bad way to reduce hash64 to 32 bits
        RgMeshInfo dstMesh = {
            .uniqueObjectID =
                uint32_t( std::hash< std::string_view >{}( srcNode->name ) % UINT32_MAX ),
            .pMeshName    = srcNode->name,
            .transform    = MakeRgTransformFromGltfNode( *srcNode ),
            .isExportable = true,
        };

        for( uint32_t i = 0; i < srcNode->mesh->primitives_count; i++ )
        {
            const cgltf_primitive& srcPrim = srcNode->mesh->primitives[ i ];

            auto vertices = GatherVertices( srcPrim, *srcNode, gltfPath );
            if( vertices.empty() )
            {
                continue;
            }

            auto indices = GatherIndices( srcPrim, *srcNode, gltfPath );
            if( indices.empty() )
            {
                continue;
            }


            RgMeshPrimitiveFlags dstFlags = 0;

            if( srcPrim.material )
            {
                if( srcPrim.material->alpha_mode == cgltf_alpha_mode_mask )
                {
                    dstFlags |= RG_MESH_PRIMITIVE_ALPHA_TESTED;
                }
                else if( srcPrim.material->alpha_mode == cgltf_alpha_mode_blend )
                {
                    debug::Warning(
                        "{}: Ignoring primitive of ...->{}->{}: Found blend material, "
                        "so it requires to be uploaded each frame, and not once on load",
                        gltfPath,
                        NodeName( srcNode->parent ),
                        NodeName( srcNode ) );
                    continue;
                    dstFlags |= RG_MESH_PRIMITIVE_TRANSLUCENT;
                }
            }


            auto matinfo = UploadTextures(
                cmd, frameIndex, srcPrim.material, textureManager, gltfFolder, gltfPath );

            auto primname = std::to_string( i );

            RgEditorInfo editorInfo = {};

            RgMeshPrimitiveInfo dstPrim = {
                .pPrimitiveNameInMesh = primname.c_str(),
                .primitiveIndexInMesh = uint32_t( i ),
                .flags                = dstFlags,
                .pVertices            = vertices.data(),
                .vertexCount          = uint32_t( vertices.size() ),
                .pIndices             = indices.empty() ? nullptr : indices.data(),
                .indexCount           = uint32_t( indices.size() ),
                .pTextureName         = matinfo.pTextureName.c_str(),
                .textureFrame         = 0,
                .color                = matinfo.color,
                .emissive             = matinfo.emissiveMult,
                .pEditorInfo          = &editorInfo,
            };

            textureMeta.Modify( dstPrim, editorInfo, true );
            {
                // pbr info from gltf has higher priority
                editorInfo.pbrInfoExists = true;
                editorInfo.pbrInfo       = { .metallicDefault  = matinfo.metallicFactor,
                                             .roughnessDefault = matinfo.roughnessFactor };
            }

            if( primitiveExtra.isGlass )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_GLASS;
            }

            if( primitiveExtra.isMirror )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_MIRROR;
            }

            if( primitiveExtra.isWater )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_WATER;
            }

            if( primitiveExtra.isSkyVisibility )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_SKY_VISIBILITY;
            }

            if( primitiveExtra.isAcid )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_ACID;
            }

            if( primitiveExtra.isThinMedia )
            {
                dstPrim.flags |= RG_MESH_PRIMITIVE_THIN_MEDIA;
            }

            auto r = scene.UploadPrimitive( frameIndex, dstMesh, dstPrim, textureManager, true );


            if( !( r == UploadResult::Static || r == UploadResult::ExportableStatic ) )
            {
                assert( 0 );
            }
        }
    }

    bool     foundLight = false;
    uint64_t counter    = 0;

    // lights
    for( cgltf_node* srcNode : std::span( mainNode->children, mainNode->children_count ) )
    {
        if( !srcNode || !srcNode->light )
        {
            continue;
        }

        if( srcNode->children_count > 0 )
        {
            debug::Warning( "{}: Found a child nodes of {}->{}. Ignoring them",
                            gltfPath,
                            mainNode->name,
                            srcNode->name );
        }

        constexpr auto candelaToLuminousFlux = []( float lumensPerSteradian ) {
            // to lumens
            return lumensPerSteradian * ( 4 * float( Utils::M_PI ) );
        };

        auto makeExtras = []( const char* extradata ) {
            return json_parser::ReadStringAs< RgLightExtraInfo >( Utils::SafeCstr( extradata ) );
        };

        RgTransform tr = MakeRgTransformFromGltfNode( *srcNode );

        RgFloat3D position = {
            tr.matrix[ 0 ][ 3 ],
            tr.matrix[ 1 ][ 3 ],
            tr.matrix[ 2 ][ 3 ],
        };

        RgFloat3D direction = {
            -tr.matrix[ 0 ][ 2 ],
            -tr.matrix[ 1 ][ 2 ],
            -tr.matrix[ 2 ][ 2 ],
        };

        RgColor4DPacked32 packedColor =
            Utils::PackColorFromFloat( RG_ACCESS_VEC3( srcNode->light->color ), 1.0f );

        // TODO: change id
        uint64_t uniqueId = UINT64_MAX - counter;
        counter++;

        switch( srcNode->light->type )
        {
            case cgltf_light_type_directional: {
                RgDirectionalLightUploadInfo info = {
                    .uniqueID               = uniqueId,
                    .isExportable           = true,
                    .extra                  = makeExtras( srcNode->light->extras.data ),
                    .color                  = packedColor,
                    .intensity              = srcNode->light->intensity, // already in lm/m^2
                    .direction              = direction,
                    .angularDiameterDegrees = 0.5f,
                };
                scene.UploadLight( frameIndex, &info, nullptr, true );
                foundLight = true;
                break;
            }
            case cgltf_light_type_point: {
                RgSphericalLightUploadInfo info = {
                    .uniqueID     = uniqueId,
                    .isExportable = true,
                    .extra        = makeExtras( srcNode->light->extras.data ),
                    .color        = packedColor,
                    .intensity =
                        candelaToLuminousFlux( srcNode->light->intensity ), // from lm/sr to lm
                    .position = position,
                    .radius   = 0.05f / oneGameUnitInMeters,
                };
                scene.UploadLight( frameIndex, &info, nullptr, true );
                foundLight = true;
                break;
            }
            case cgltf_light_type_spot: {
                RgSpotLightUploadInfo info = {
                    .uniqueID     = uniqueId,
                    .isExportable = true,
                    .extra        = makeExtras( srcNode->light->extras.data ),
                    .color        = packedColor,
                    .intensity =
                        candelaToLuminousFlux( srcNode->light->intensity ), // from lm/sr to lm
                    .position   = position,
                    .direction  = direction,
                    .radius     = 0.05f / oneGameUnitInMeters,
                    .angleOuter = srcNode->light->spot_outer_cone_angle,
                    .angleInner = srcNode->light->spot_inner_cone_angle,
                };
                scene.UploadLight( frameIndex, &info, nullptr, true );
                foundLight = true;
                break;
            }
            case cgltf_light_type_invalid:
            case cgltf_light_type_max_enum:
            default: break;
        }
    }

    if( !foundLight )
    {
        debug::Warning( "Haven't found any lights in {}: "
                        "Original exportable lights will be used",
                        gltfPath );
    }
}

RTGL1::GltfImporter::operator bool() const
{
    return data != nullptr;
}
