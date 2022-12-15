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

#include "GltfExporter.h"

#include "Utils.h"

#define CGLTF_VALIDATE_ENABLE_ASSERTS 1
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION

#include "cgltf/cgltf_write.h"

#include <array>
#include <cassert>
#include <fstream>
#include <queue>
#include <span>
#include <type_traits>

namespace 
{
#define RTGL1_MAIN_ROOT_NODE "rtgl1_main_root"

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



RTGL1::Exporter::Exporter( const RgFloat3D& _worldUp,
                           const RgFloat3D& _worldForward,
                           float            _worldScale,
                           DebugPrintFn     _debugprint )
    : worldTransform( Utils::MakeTransform(
          Utils::Normalize( _worldUp ), Utils::Normalize( _worldForward ), _worldScale ) )
    , debugprint( std::move( _debugprint ) )
{
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


struct BeginCount
{
    ptrdiff_t offset;
    size_t count;

    template< typename T > std::span< T > ToSpan( std::vector< T >& container ) const
    {
        return { container.begin() + offset, count };
    }

    template< typename T > T* ToPointer( std::vector< T >& container ) const
    {
        assert( count == 1 );
        return &container[ offset ];
    }

    template< typename T > std::vector< T* > ToVectorOfPointers( std::vector< T >& container ) const
    {
        std::vector< T* > ptrs;

        std::ranges::transform(
            ToSpan< T >( container ), std::back_inserter( ptrs ), ConvertRefToPtr< T > );

        return ptrs;
    }
};

template< typename T > auto append_n( std::vector< T >& v, size_t toadd )
{
    auto begin = ptrdiff_t( v.size() );
    v.resize( v.size() + toadd );
    
    return BeginCount{ begin, toadd };
}


struct GltfRoot
{
    std::string                name;
    cgltf_node*                parent;
    std::vector< cgltf_node* > children;

    std::span< cgltf_buffer_view > bufferViews;
    std::span< cgltf_accessor >    accessors;
    std::span< cgltf_attribute >   attributes;
    std::span< cgltf_primitive >   primitives;
    std::span< cgltf_mesh >        meshes;

    std::vector< std::shared_ptr< RTGL1::DeepCopyOfPrimitive > > source;
};

struct GltfStorage
{
    explicit GltfStorage(
        const rgl::unordered_map< std::string,
                                  std::vector< std::shared_ptr< RTGL1::DeepCopyOfPrimitive > > >&
            scene )
    {
        struct Ranges
        {
            BeginCount bufferViews;
            BeginCount accessors;
            BeginCount attributes;
            BeginCount primitives;
            BeginCount meshes;
            BeginCount parent;
            BeginCount children;
        };
        std::queue< Ranges > ranges;

        // alloc
        for( const auto& [ meshname, prims ] : scene )
        {
            ranges.push( Ranges{
                .bufferViews = append_n( allBufferViews, prims.size() * BufferViewsPerPrim ),
                .accessors   = append_n( allAccessors, prims.size() * AccessorsPerPrim ),
                .attributes  = append_n( allAttributes, prims.size() * AttributesPerPrim ),
                .primitives  = append_n( allPrimitives, prims.size() ),
                .meshes      = append_n( allMeshes, prims.size() ),
                .parent      = append_n( allNodes, 1 ),
                .children    = append_n( allNodes, prims.size() ),
            } );
        }
        BeginCount worldbc = append_n( allNodes, 1 );

        // resolve pointers
        for( const auto& [ meshName, prims ] : scene )
        {
            const Ranges r = ranges.front();
            ranges.pop();

            GltfRoot root = {
                .name        = meshName,
                .parent      = r.parent.ToPointer( allNodes ),
                .children    = r.children.ToVectorOfPointers( allNodes ),
                .bufferViews = r.bufferViews.ToSpan( allBufferViews ),
                .accessors   = r.accessors.ToSpan( allAccessors ),
                .attributes  = r.attributes.ToSpan( allAttributes ),
                .primitives  = r.primitives.ToSpan( allPrimitives ),
                .meshes      = r.meshes.ToSpan( allMeshes ),
                .source      = prims,
            };

            assert( root.source.size() == root.children.size() );
            assert( root.source.size() == root.primitives.size() );
            assert( root.source.size() == root.meshes.size() );

            worldRoots.push_back( root.parent );
            roots.push_back( std::move( root ) );
        }
        world = worldbc.ToPointer( allNodes );

        assert( ranges.empty() );
        assert( worldRoots.size() == roots.size() );
    }

    std::vector< cgltf_buffer_view > allBufferViews;
    std::vector< cgltf_accessor >    allAccessors;
    std::vector< cgltf_attribute >   allAttributes;
    std::vector< cgltf_primitive >   allPrimitives;
    std::vector< cgltf_mesh >        allMeshes;
    std::vector< cgltf_node >        allNodes;

    std::vector< GltfRoot >    roots; // each corresponds to RgMeshInfo

    cgltf_node*                world; 
    std::vector< cgltf_node* > worldRoots;
};


}


void RTGL1::Exporter::AddPrimitive( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive )
{
    if ( !mesh.isExportable )
    {
        return;
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
    
    
    const char* primExtrasExample  = nullptr; // "{ portalOutPosition\" : [0,0,0] }";
    const char* sceneExtrasExample = nullptr; // "{ tonemapping_enable\" : 1 }";

    
    // lock pointers
    BinFile fbin( folder, sceneName );
    GltfStorage storage( scene );
    

    for( GltfRoot& root : storage.roots )
    {
        *root.parent = cgltf_node{
            .name           = const_cast< char* >( root.name.c_str() ),
            .parent         = nullptr,
            .children       = std::data( root.children ),
            .children_count = std::size( root.children ),
            .extras         = { .data = nullptr },
        };

        for( size_t i = 0; i < root.source.size(); i++ )
        {
            const DeepCopyOfPrimitive& rgprim = *root.source[ i ];

            std::span viewsDst( root.bufferViews.begin() + ptrdiff_t( BufferViewsPerPrim * i ),
                                BufferViewsPerPrim );
            {
                std::ranges::swap_ranges( viewsDst, MakeBufferViews( fbin, rgprim ) );
            }

            std::span accessorsDst( root.accessors.begin() + ptrdiff_t( AccessorsPerPrim * i ),
                                    AccessorsPerPrim );
            {
                std::ranges::swap_ranges(
                    accessorsDst,
                    MakeAccessors( rgprim.Vertices().size(), rgprim.Indices().size(), viewsDst ) );
            }

            std::span vertAttrsDst( root.attributes.begin() + ptrdiff_t( AttributesPerPrim * i ),
                                    AttributesPerPrim );
            {
                std::ranges::swap_ranges( vertAttrsDst, MakeVertexAttributes( accessorsDst ) );
            }

            root.primitives[ i ] = cgltf_primitive{
                .type             = cgltf_primitive_type_triangles,
                .indices          = GetIndicesAccessor( accessorsDst ),
                .material         = nullptr,
                .attributes       = std::data( vertAttrsDst ),
                .attributes_count = std::size( vertAttrsDst ),
                .extras           = {},
            };

            root.meshes[ i ] = cgltf_mesh{
                .name             = const_cast< char* >( rgprim.PrimitiveNameInMesh().c_str() ),
                .primitives       = &root.primitives[ i ],
                .primitives_count = 1,
                .extras           = {},
            };

            *root.children[ i ] = cgltf_node{
                .name           = const_cast< char* >( rgprim.PrimitiveNameInMesh().c_str() ),
                .parent         = root.parent,
                .children       = nullptr,
                .children_count = 0,
                .mesh           = &root.meshes[ i ],
                .camera         = nullptr,
                .light          = nullptr,
                .has_matrix     = true,
                .matrix         = RG_TRANSFORM_TO_GLTF_MATRIX( rgprim.Transform() ),
                .extras         = { .data = const_cast< char* >( primExtrasExample ) },
                .has_mesh_gpu_instancing = false,
                .mesh_gpu_instancing     = {},
            };
        }
    }

    // main root node
    {
        *storage.world = cgltf_node{
            .name           = const_cast< char* >( RTGL1_MAIN_ROOT_NODE ),
            .parent         = nullptr,
            .children       = std::data( storage.worldRoots ),
            .children_count = std::size( storage.worldRoots ),
            .has_matrix     = true,
            .matrix         = RG_TRANSFORM_TO_GLTF_MATRIX( worldTransform ),
            .extras         = { .data = nullptr },
        };
        for( cgltf_node* child : storage.worldRoots )
        {
            child->parent = storage.world;
        }
    }

    cgltf_scene gltfScene = {
        .name        = const_cast< char* >( "default" ),
        .nodes       = &storage.world,
        .nodes_count = 1,
        .extras      = { .data = const_cast< char* >( sceneExtrasExample ) },
    };

    // Note: const_cast, assuming that cgltf_write_file makes only read-only access
    cgltf_data data = {
        .asset =
            cgltf_asset{
                .copyright   = nullptr,
                .generator   = const_cast< char* >( "RTGL1" ),
                .version     = const_cast< char* >( "2.0" ),
                .min_version = nullptr,
            },
        .meshes             = std::data( storage.allMeshes ),
        .meshes_count       = std::size( storage.allMeshes ),
        .accessors          = std::data( storage.allAccessors ),
        .accessors_count    = std::size( storage.allAccessors ),
        .buffer_views       = std::data( storage.allBufferViews ),
        .buffer_views_count = std::size( storage.allBufferViews ),
        .buffers            = fbin.Get(),
        .buffers_count      = 1,
        .nodes              = std::data( storage.allNodes ),
        .nodes_count        = std::size( storage.allNodes ),
        .scenes             = &gltfScene,
        .scenes_count       = 1,
        .scene              = &gltfScene,
    };

    cgltf_options options = {};
    cgltf_result  r;

    r = cgltf_validate( &data );
    if( r != cgltf_result_success)
    {
        debugprint( "cgltf_validate fail", RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    const auto gltfPath = GetGltfPath( folder, sceneName ).string();

    r = cgltf_write_file( &options, gltfPath.c_str(), &data );
    if( r != cgltf_result_success )
    {
        debugprint( "cgltf_write_file fail", RG_MESSAGE_SEVERITY_WARNING );
        return;
    }

    debugprint( ( gltfPath + ": Exported successfully" ).c_str(), RG_MESSAGE_SEVERITY_INFO );
}
