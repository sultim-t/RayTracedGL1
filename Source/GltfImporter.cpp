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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace
{

// clang-format off
// Column memory order!
#define RG_TRANSFORM_TO_GLTF_MATRIX( t ) {                                      \
    ( t ).matrix[ 0 ][ 0 ], ( t ).matrix[ 1 ][ 0 ], ( t ).matrix[ 2 ][ 0 ], 0,  \
    ( t ).matrix[ 0 ][ 1 ], ( t ).matrix[ 1 ][ 1 ], ( t ).matrix[ 2 ][ 1 ], 0,  \
    ( t ).matrix[ 0 ][ 2 ], ( t ).matrix[ 1 ][ 2 ], ( t ).matrix[ 2 ][ 2 ], 0,  \
    ( t ).matrix[ 0 ][ 3 ], ( t ).matrix[ 1 ][ 3 ], ( t ).matrix[ 2 ][ 3 ], 1   }
// clang-format on

std::string GetErrorMessage( std::string_view gltfPath,
                             std::string_view msg,
                             std::optional< cgltf_result > result = std::nullopt )
{
    return std::string() + gltfPath.data() + ": " + msg.data() +
           ( result ? ". Error code " + std::to_string( *result ) : "" );
}

glm::mat4 MakeGltfTransform( const cgltf_node& node )
{
    if( node.has_matrix )
    {
       return glm::mat4( glm::make_mat4x4( node.matrix ) );
    }
    else
    {
        glm::mat4 m( 1.0f );
        if( node.has_scale )
        {
            m *= glm::scale( glm::mat4( 1.0f ), glm::make_vec3( node.scale ) );
        }
        if( node.has_rotation )
        {
            m *= glm::toMat4( glm::make_quat( node.rotation ) );
        }
        if( node.has_translation )
        {
            m *= glm::translate( glm::mat4( 1.0f ), glm::make_vec3( node.translation ) );
        }
        return m;
    }
}

void ApplyInverseWorldTransform( cgltf_node &mainNode, const RgTransform& worldTransform )
{
    const float gltfMatrixWorld[] = RG_TRANSFORM_TO_GLTF_MATRIX( worldTransform );
    
    glm::mat4 newMatrix =
        glm::inverse( glm::make_mat4x4( gltfMatrixWorld ) ) * 
        MakeGltfTransform( mainNode );

	mainNode.has_translation = false;
    memset( mainNode.translation, 0, sizeof( mainNode.translation ) );

    mainNode.has_rotation = false;
    memset( mainNode.rotation, 0, sizeof( mainNode.rotation ) );

    mainNode.has_scale = false;
    memset( mainNode.scale, 0, sizeof( mainNode.scale ) );

    mainNode.has_matrix = true;

    // direct memcpy, as glm and gltf are column-major
    static_assert( sizeof( newMatrix ) == sizeof( mainNode.matrix ) );
    memcpy( mainNode.matrix, &newMatrix[ 0 ][ 0 ], sizeof( mainNode.matrix ) );
}

}

RTGL1::GltfImporter::GltfImporter( const std::filesystem::path& _gltfPath,
                                   const RgTransform&           _worldTransform,
                                   DebugPrintFn                 _debugprint )
    : data( nullptr ), debugprint( std::move( _debugprint ) )
{
    std::string utf8Path = _gltfPath.string();

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

    r = cgltf_parse_file( &options, utf8Path.c_str(), &parsedData );
    if( r != cgltf_result_success)
    {
        debugprint( GetErrorMessage( utf8Path, "cgltf_parse_file", r ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    r = cgltf_load_buffers( &options, parsedData, utf8Path.c_str() );
    if( r != cgltf_result_success )
    {
        debugprint( GetErrorMessage( utf8Path, "cgltf_load_buffers", r ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    r = cgltf_validate( parsedData );
    if( r != cgltf_result_success )
    {
        debugprint( GetErrorMessage( utf8Path, "cgltf_validate", r ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    if( parsedData->scenes_count == 0 )
    {
        debugprint( GetErrorMessage( utf8Path, "No scenes found" ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    if( parsedData->scene == nullptr )
    {
        debugprint( GetErrorMessage( utf8Path, "No default scene, using first" ).c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        parsedData->scene = &parsedData->scenes[ 0 ];
    }

    cgltf_node* mainnode = nullptr;
    for( size_t i = 0; i < parsedData->scene->nodes_count; i++ )
    {
        cgltf_node* n = parsedData->scene->nodes[ i ];

        if( std::strcmp( n->name, RTGL1_MAIN_ROOT_NODE ) == 0 )
        {
            mainnode = n;
            break;
        }
    }

    if( !mainnode )
    {
        debugprint(
            GetErrorMessage( utf8Path, "Default scene: no node named " RTGL1_MAIN_ROOT_NODE )
                .c_str(),
            RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    ApplyInverseWorldTransform( *mainnode, _worldTransform );

    data = parsedData;
}

RTGL1::GltfImporter::~GltfImporter()
{
    cgltf_free( data );
}


