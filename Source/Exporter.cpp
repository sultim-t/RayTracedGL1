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

#include <array>
#include <cassert>
#include <fstream>
#include <span>
#include <type_traits>

namespace 
{
auto* ConvertRefToPtr( auto& ref )
{
    return &ref;
}

bool IsCstrEmpty(const char *cstr)
{
    return cstr == nullptr || *cstr == '\0';
}

std::string SafeString( const char* cstr )
{
    return IsCstrEmpty( cstr ) ? "" : cstr;
}

std::string SafeSceneName( const char* pSceneName )
{
    return IsCstrEmpty( pSceneName ) ? "scene" : pSceneName;
}

std::filesystem::path GetGltfPath( const std::filesystem::path& folder, const char* pSceneName )
{
    return folder / ( SafeSceneName( pSceneName ) + ".gltf" );
}

std::string GetGltfBinURI( const char* pSceneName )
{
    return SafeSceneName( pSceneName ) + ".bin";
}

std::filesystem::path GetGltfBinPath( const std::filesystem::path& folder, const char* pSceneName )
{
    return folder / GetGltfBinURI( pSceneName );
}

// clang-format off
#define RG_TRANSFORM_TO_GLTF_MATRIX( t ) {                                  \
    ( t ).matrix[ 0 ][ 0 ], ( t ).matrix[ 0 ][ 1 ], ( t ).matrix[ 0 ][ 2 ], \
    ( t ).matrix[ 0 ][ 3 ], ( t ).matrix[ 1 ][ 0 ], ( t ).matrix[ 1 ][ 1 ], \
    ( t ).matrix[ 1 ][ 2 ], ( t ).matrix[ 1 ][ 3 ], ( t ).matrix[ 2 ][ 0 ], \
    ( t ).matrix[ 2 ][ 1 ], ( t ).matrix[ 2 ][ 2 ], ( t ).matrix[ 2 ][ 3 ], \
    0.f, 0.f, 0.f, 1.f }
// clang-format on
}



namespace RTGL1
{
struct DeepCopyOfPrimitive
{
    explicit DeepCopyOfPrimitive( const RgMeshPrimitiveInfo& c ) : info( c )
    {
        std::span fromVertices( c.pVertices, c.vertexCount );
        std::span fromIndices( c.pIndices, c.indexCount );
        // for copying
        static_assert(
            std::is_same_v< decltype( c.pVertices ), decltype( pVertices )::const_pointer > );
        static_assert(
            std::is_same_v< decltype( c.pIndices ), decltype( pIndices )::const_pointer > );
        static_assert( std::is_trivially_copyable_v< decltype( pVertices )::value_type > );
        static_assert( std::is_trivially_copyable_v< decltype( pIndices )::value_type > );

        // deep copy
        pPrimitiveNameInMesh = SafeString( c.pPrimitiveNameInMesh );
        pTextureName         = SafeString( c.pTextureName );
        pVertices.assign( fromVertices.begin(), fromVertices.end() );
        pIndices.assign( fromIndices.begin(), fromIndices.end() );

        FixupPointers( *this );
    }
    ~DeepCopyOfPrimitive() = default;

    DeepCopyOfPrimitive( DeepCopyOfPrimitive&& other ) noexcept
        : info( other.info )
        , pPrimitiveNameInMesh( std::move( other.pPrimitiveNameInMesh ) )
        , pTextureName( std::move( other.pTextureName ) )
        , pVertices( std::move( other.pVertices ) )
        , pIndices( std::move( other.pIndices ) )              
    {
        assert( other.info.vertexCount == other.pVertices.size() );
        assert( other.info.indexCount == other.pIndices.size() );

        FixupPointers( *this );
    }

    DeepCopyOfPrimitive& operator=( DeepCopyOfPrimitive&& other ) noexcept
    {
        assert( other.info.vertexCount == other.pVertices.size() );
        assert( other.info.indexCount == other.pIndices.size() );

        this->pPrimitiveNameInMesh = std::move( other.pPrimitiveNameInMesh );
        this->pTextureName         = std::move( other.pTextureName );
        this->pVertices            = std::move( other.pVertices );
        this->pIndices             = std::move( other.pIndices );

        // copy and fix
        this->info = other.info;
        FixupPointers( *this );

        // invalidate other
        other.info = {};

        return *this;
    }

    // No copies
    DeepCopyOfPrimitive( const DeepCopyOfPrimitive& other ) = delete;
    DeepCopyOfPrimitive& operator=( const DeepCopyOfPrimitive& other ) = delete;
    
    std::span< const RgPrimitiveVertex > Vertices() const
    {
        return { pVertices.begin(), pVertices.end() };
    }
    
    std::span< const uint32_t > Indices() const
    {
        return { pIndices.begin(), pIndices.end() };
    }

    const auto& PrimitiveNameInMesh() const { return pPrimitiveNameInMesh; }
    const auto& TextureName() const { return pTextureName; }
    const auto& Transform() const { return info.transform; }

private:
    static void FixupPointers( DeepCopyOfPrimitive& inout )
    {
        inout.info.pPrimitiveNameInMesh = inout.pPrimitiveNameInMesh.data();
        inout.info.pTextureName         = inout.pTextureName.data();
        inout.info.pVertices            = inout.pVertices.data();
        inout.info.pIndices             = inout.pIndices.data();

        assert( inout.info.vertexCount == inout.pVertices.size() );
        assert( inout.info.indexCount == inout.pIndices.size() );
    }

private:
    RgMeshPrimitiveInfo info;

    // to maintain lifetimes
    std::string                      pPrimitiveNameInMesh;
    std::string                      pTextureName;
    std::vector< RgPrimitiveVertex > pVertices;
    std::vector< uint32_t >          pIndices;
};
}



RTGL1::Exporter::Exporter(
    std::function< void( const char*, RgMessageSeverityFlags ) > _debugprint )
    : debugprint( std::move( _debugprint ) )
{
}

RTGL1::Exporter::~Exporter()
{
    // TODO: check if ExportToFiles was called
    assert( 1 );
}

namespace
{

struct BinFile
{
    explicit BinFile( const std::filesystem::path& folder, const char* pSceneName )
        : uri( GetGltfBinURI( pSceneName ) )
        , file( GetGltfBinPath( folder, pSceneName ),
                std::ios::out | std::ios::trunc | std::ios::binary )
        , fileOffset( 0 )
        , storage{}
    {
    }

    cgltf_buffer* Get()
    {
        storage = cgltf_buffer{
            .name = nullptr,
            .size = fileOffset,
            .uri  = const_cast< char* >( uri.c_str() ),
        };
        return &storage;
    }

    // Returns begin of written data.
    template< typename T >
    size_t Write( std::span< T > bytes )
    {
        size_t begin = fileOffset;

        file.write( reinterpret_cast< const char* >( bytes.data() ),
                    std::streamsize( bytes.size_bytes() ) );
        fileOffset += bytes.size_bytes();

        return begin;
    }

private:
    std::string   uri;
    std::ofstream file;
    size_t        fileOffset;
    cgltf_buffer  storage;
};


auto MakeBufferViews( BinFile& fbin, const RTGL1::DeepCopyOfPrimitive& prim )
{
    return std::to_array( {
        #define BUFFER_VIEW_VERTICES 0
        cgltf_buffer_view{
            .name   = nullptr,
            .buffer = fbin.Get(),
            .offset = fbin.Write( prim.Vertices() ),
            .size   = prim.Vertices().size_bytes(),
            .stride = sizeof( decltype( prim.Vertices() )::element_type ),
            .type   = cgltf_buffer_view_type_vertices,
        },
        #define BUFFER_VIEW_INDICES 1
        cgltf_buffer_view{
            .name   = nullptr,
            .buffer = fbin.Get(),
            .offset = fbin.Write( prim.Indices() ),
            .size   = prim.Indices().size_bytes(),
            .stride = sizeof( decltype( prim.Indices() )::element_type ),
            .type   = cgltf_buffer_view_type_indices,
        },
    } );
}
constexpr size_t BufferViewsPerPrim =
    std::size( std::invoke_result_t< decltype( MakeBufferViews ),
                                     BinFile&,
                                     const RTGL1::DeepCopyOfPrimitive& >{} );

auto MakeAccessors( size_t                         vertexCount,
                    size_t                         indexCount,
                    std::span< cgltf_buffer_view > correspondingViews )
{
    assert( correspondingViews.size() == BufferViewsPerPrim );

    return std::to_array( {
        #define ACCESSOR_POSITION 0
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = false,
            .type           = cgltf_type_vec3,
            .offset         = offsetof( RgPrimitiveVertex, position ),
            .count          = vertexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_VERTICES ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
        #define ACCESSOR_NORMAL 1
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = true,
            .type           = cgltf_type_vec3,
            .offset         = offsetof( RgPrimitiveVertex, normal ),
            .count          = vertexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_VERTICES ],
            .has_min        = true,
            .min            = { -1.f, -1.f, -1.f },
            .has_max        = true,
            .max            = { 1.f, 1.f, 1.f },
        },
        #define ACCESSOR_TANGENT 2
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = true,
            .type           = cgltf_type_vec4,
            .offset         = offsetof( RgPrimitiveVertex, tangent ),
            .count          = vertexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_VERTICES ],
            .has_min        = true,
            .min            = { -1.f, -1.f, -1.f, -1.f },
            .has_max        = true,
            .max            = { 1.f, 1.f, 1.f, 1.f },
        },
        #define ACCESSOR_TEXCOORD 3
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32f,
            .normalized     = false,
            .type           = cgltf_type_vec2,
            .offset         = offsetof( RgPrimitiveVertex, texCoord ),
            .count          = vertexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_VERTICES ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
        #define ACCESSOR_COLOR 4
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_8u,
            .normalized     = false,
            .type           = cgltf_type_vec4,
            .offset         = offsetof( RgPrimitiveVertex, color ),
            .count          = vertexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_VERTICES ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
        #define ACCESSOR_INDEX 5
        cgltf_accessor{
            .name           = nullptr,
            .component_type = cgltf_component_type_r_32u,
            .normalized     = false,
            .type           = cgltf_type_scalar,
            .offset         = 0,
            .count          = indexCount,
            .buffer_view    = &correspondingViews[ BUFFER_VIEW_INDICES ],
            .has_min        = false,
            .min            = {},
            .has_max        = false,
            .max            = {},
        },
    } );
}
constexpr size_t AccessorsPerPrim =
    std::size( std::invoke_result_t< decltype( MakeAccessors ),
                                     size_t,
                                     size_t,
                                     std::span< cgltf_buffer_view > >{} );
cgltf_accessor* GetIndicesAccessor( std::span< cgltf_accessor > correspondingAccessors )
{
    return &correspondingAccessors[ ACCESSOR_INDEX ];
}


auto MakeVertexAttributes( std::span< cgltf_accessor > correspondingAccessors )
{
    assert( correspondingAccessors.size() == AccessorsPerPrim );

    return std::to_array( {
        cgltf_attribute{
            .name  = const_cast< char* >( "POSITION" ),
            .type  = cgltf_attribute_type_position,
            .index = 0,
            .data  = &correspondingAccessors[ ACCESSOR_POSITION ],
        },
        cgltf_attribute{
            .name  = const_cast< char* >( "NORMAL" ),
            .type  = cgltf_attribute_type_normal,
            .index = 0,
            .data  = &correspondingAccessors[ ACCESSOR_NORMAL ],
        },
        cgltf_attribute{
            .name  = const_cast< char* >( "TANGENT" ),
            .type  = cgltf_attribute_type_tangent,
            .index = 0,
            .data  = &correspondingAccessors[ ACCESSOR_TANGENT ],
        },
        cgltf_attribute{
            .name  = const_cast< char* >( "TEXCOORD_0" ),
            .type  = cgltf_attribute_type_texcoord,
            .index = 0,
            .data  = &correspondingAccessors[ ACCESSOR_TEXCOORD ],
        },
        cgltf_attribute{
            .name  = const_cast< char* >( "COLOR" ),
            .type  = cgltf_attribute_type_color,
            .index = 0,
            .data  = &correspondingAccessors[ ACCESSOR_COLOR ],
        },
    } );
}
constexpr size_t AttributesPerPrim = std::size(
    std::invoke_result_t< decltype( MakeVertexAttributes ), std::span< cgltf_accessor > >{} );


}


void RTGL1::Exporter::AddPrimitive( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive )
{
    if ( !mesh.isExportable )
    {
    //    return;
    }

    if( IsCstrEmpty( mesh.pMeshName ) || IsCstrEmpty( primitive.pPrimitiveNameInMesh ) )
    {
        return;
    }

    if( primitive.indexCount == 0 || primitive.pIndices == nullptr )
    {
        debugprint( ( std::string( "Exporter doesn't support primitives without index buffer: " ) +
                      mesh.pMeshName + " - " + primitive.pPrimitiveNameInMesh )
                        .c_str(),
                    RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    scene[ mesh.pMeshName ].emplace_back( std::make_shared< DeepCopyOfPrimitive >( primitive ) );
}

void RTGL1::Exporter::ExportToFiles( const std::filesystem::path& folder )
{
    if( scene.empty() )
    {
        debugprint( "Nothing to export", RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    const char *sceneName = nullptr;

    const auto [ rgmeshName, rgprimitives ] = *scene.begin();
    
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

    
    // lock pointer
    BinFile fbin( folder, sceneName );


    // lock pointers - no resize!
    // clang-format off
    auto bufferViews = std::vector< cgltf_buffer_view   >( BufferViewsPerPrim * rgprimitives.size() );
    auto accessors   = std::vector< cgltf_accessor      >( AccessorsPerPrim * rgprimitives.size() );
    auto vertAttrs   = std::vector< cgltf_attribute     >( AttributesPerPrim * rgprimitives.size() );
    auto gltfPrims   = std::vector< cgltf_primitive     >( rgprimitives.size() );
    auto gltfMeshes  = std::vector< cgltf_mesh          >( rgprimitives.size() );
    // clang-format on
    data.buffer_views       = std::data( bufferViews );
    data.buffer_views_count = std::size( bufferViews );
    data.accessors          = std::data( accessors );
    data.accessors_count    = std::size( accessors );
    data.meshes             = std::data( gltfMeshes );
    data.meshes_count       = std::size( gltfMeshes );

    
    // lock pointers - no resize!
    auto allNodes = std::vector< cgltf_node >( 1 /* parent */ + gltfMeshes.size() );
    
    cgltf_node* parentNode = &allNodes[ 0 ];
    std::vector< cgltf_node* > childNodes;
    std::ranges::transform( std::span( allNodes.begin() + 1, gltfMeshes.size() ),
                            std::back_inserter( childNodes ),
                            ConvertRefToPtr< cgltf_node > );
    *parentNode = cgltf_node{
        .name           = const_cast< char* >( rgmeshName.c_str() ),
        .parent         = nullptr,
        .children       = std::data( childNodes ),
        .children_count = std::size( childNodes ),
        .extras         = { .data = nullptr },
    };

    data.nodes       = std::data( allNodes );
    data.nodes_count = std::size( allNodes );


    for( size_t i = 0; i < rgprimitives.size(); i++ )
    {
        const DeepCopyOfPrimitive& rgprim = *rgprimitives[ i ];

        std::span viewsDst( bufferViews.begin() + ptrdiff_t( BufferViewsPerPrim * i ),
                            BufferViewsPerPrim );
        {
            std::ranges::swap_ranges( viewsDst, MakeBufferViews( fbin, rgprim ) );
        }

        std::span accessorsDst( accessors.begin() + ptrdiff_t( AccessorsPerPrim * i ),
                                AccessorsPerPrim );
        {
            std::ranges::swap_ranges(
                accessorsDst,
                MakeAccessors( rgprim.Vertices().size(), rgprim.Indices().size(), viewsDst ) );
        }

        std::span vertAttrsDst( vertAttrs.begin() + ptrdiff_t( AttributesPerPrim * i ),
                                AttributesPerPrim );
        {
            std::ranges::swap_ranges( vertAttrsDst, MakeVertexAttributes( accessorsDst ) );
        }

        gltfPrims[ i ] = cgltf_primitive{
            .type             = cgltf_primitive_type_triangles,
            .indices          = GetIndicesAccessor( accessorsDst ),
            .material         = nullptr,
            .attributes       = std::data( vertAttrsDst ),
            .attributes_count = std::size( vertAttrsDst ),
            .extras           = {},
        };

        gltfMeshes[ i ] = cgltf_mesh{
            .name             = const_cast< char* >( rgprim.PrimitiveNameInMesh().c_str() ),
            .primitives       = &gltfPrims[ i ],
            .primitives_count = 1,
            .extras           = {},
        };

        *childNodes[ i ] = cgltf_node{
            .name                    = const_cast< char* >( rgprim.PrimitiveNameInMesh().c_str() ),
            .parent                  = parentNode,
            .children                = nullptr,
            .children_count          = 0,
            .mesh                    = &gltfMeshes[ i ],
            .camera                  = nullptr,
            .light                   = nullptr,
            .has_matrix              = true,
            .matrix                  = RG_TRANSFORM_TO_GLTF_MATRIX( rgprim.Transform() ),
            .extras                  = { .data = const_cast< char* >( primExtrasExample ) },
            .has_mesh_gpu_instancing = false,
            .mesh_gpu_instancing     = {},
        };
    }
    
    cgltf_scene scenes[] = {
        {
            .name        = const_cast< char* >( "default" ),
            .nodes       = &parentNode,
            .nodes_count = 1,
            .extras      = { .data = const_cast< char* >( sceneExtrasExample ) },
        },
    };
    data.scenes       = std::data( scenes );
    data.scenes_count = std::size( scenes );
    data.scene        = &scenes[ 0 ];

    data.buffers       = fbin.Get();
    data.buffers_count = 1;

    cgltf_options options = {};
    cgltf_result  r;

    r = cgltf_validate( &data );
    if( r != cgltf_result_success)
    {
        debugprint( "cgltf_validate fail", RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    r = cgltf_write_file( &options, GetGltfPath( folder, sceneName ).string().c_str(), &data );
    if( r != cgltf_result_success )
    {
        debugprint( "cgltf_write_file fail", RG_MESSAGE_SEVERITY_WARNING );
        return;
    }
}
