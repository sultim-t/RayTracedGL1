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

#include "Const.h"
#include "TextureExporter.h"
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
template< class T >
void HashCombine( std::size_t& seed, const T& v )
{
    seed ^= std::hash< T >{}( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

bool TransformIsLess( const RgTransform& a, const RgTransform& b )
{
    for( size_t i = 0; i < std::size( a.matrix ); i++ )
    {
        for( size_t j = 0; j < std::size( a.matrix[ 0 ] ); j++ )
        {
            if( a.matrix[ i ][ j ] < b.matrix[ i ][ j ] )
            {
                return true;
            }
        }
    }
    return false;
}

bool TransformsAreEqual( const RgTransform& a, const RgTransform& b )
{
    constexpr float eps = 0.0001f;
    for( size_t i = 0; i < std::size( a.matrix ); i++ )
    {
        for( size_t j = 0; j < std::size( a.matrix[ 0 ] ); j++ )
        {
            if( std::abs( a.matrix[ i ][ j ] - b.matrix[ i ][ j ] ) > eps )
            {
                return false;
            }
        }
    }
    return true;
}

uint64_t TransformHash( const RgTransform& a )
{
    uint64_t h = 0;
    for( size_t i = 0; i < std::size( a.matrix ); i++ )
    {
        for( size_t j = 0; j < std::size( a.matrix[ 0 ] ); j++ )
        {
            HashCombine( h, a.matrix[ i ][ j ] );
        }
    }
    return h;
}
}

template<>
struct std::hash< RgTransform >
{
    std::size_t operator()( RgTransform const& t ) const noexcept { return TransformHash( t ); }
};

namespace
{
template< typename T >
    requires( std::is_default_constructible_v< T > )
T ValueOrDefault( const std::optional< T > v )
{
    return v.value_or( T{} );
}

auto* ConvertRefToPtr( auto& ref )
{
    return &ref;
}

std::filesystem::path GetGltfFolder( const std::filesystem::path& gltfPath )
{
    return gltfPath.parent_path();
}

std::filesystem::path GetGltfBinPath( std::filesystem::path gltfPath )
{
    return gltfPath.replace_extension( ".bin" );
}

std::string GetGltfBinURI( const std::filesystem::path& gltfPath )
{
    return GetGltfBinPath( gltfPath ).filename().string();
}
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
        pPrimitiveNameInMesh = Utils::SafeCstr( c.pPrimitiveNameInMesh );
        pTextureName         = Utils::SafeCstr( c.pTextureName );
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
    DeepCopyOfPrimitive( const DeepCopyOfPrimitive& other )            = delete;
    DeepCopyOfPrimitive& operator=( const DeepCopyOfPrimitive& other ) = delete;

    std::span< const RgPrimitiveVertex > Vertices() const
    {
        return { pVertices.begin(), pVertices.end() };
    }

    std::span< const uint32_t > Indices() const { return { pIndices.begin(), pIndices.end() }; }

    std::string_view PrimitiveNameInMesh() const { return pPrimitiveNameInMesh; }
    std::string_view MaterialName() const { return pTextureName; }

    RgFloat4D Color() const { return Utils::UnpackColor4DPacked32( info.color ); }
    float     Emissive() const { return info.emissive; }

    cgltf_alpha_mode AlphaMode() const
    {
        if( info.flags & RG_MESH_PRIMITIVE_ALPHA_TESTED )
        {
            return cgltf_alpha_mode_mask;
        }
        if( ( info.flags & RG_MESH_PRIMITIVE_TRANSLUCENT ) ||
            Utils::UnpackAlphaFromPacked32( info.color ) < MESH_TRANSLUCENT_ALPHA_THRESHOLD )
        {
            return cgltf_alpha_mode_blend;
        }
        return cgltf_alpha_mode_opaque;
    }

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

namespace
{
cgltf_material MakeMaterial( const RTGL1::DeepCopyOfPrimitive& rgprim,
                             cgltf_texture*                    albedo,
                             cgltf_texture*                    normal,
                             cgltf_texture*                    metalrough )
{
    std::optional metallicRoughness = cgltf_pbr_metallic_roughness{
        .base_color_texture         = { .texture = albedo, .texcoord = 0 },
        .metallic_roughness_texture = { .texture = metalrough, .texcoord = 0 },
        .base_color_factor          = { RG_ACCESS_VEC4( rgprim.Color().data ) },
        .metallic_factor            = 0.0f,
        .roughness_factor           = 1.0f,
    };

    return cgltf_material{
        .name                        = const_cast< char* >( rgprim.MaterialName().data() ),
        .has_pbr_metallic_roughness  = metallicRoughness.has_value(),
        .has_pbr_specular_glossiness = false,
        .has_clearcoat               = false,
        .has_transmission            = false,
        .has_volume                  = false,
        .has_ior                     = false,
        .has_specular                = false,
        .has_sheen                   = false,
        .has_emissive_strength       = false,
        .has_iridescence             = false,
        .pbr_metallic_roughness      = ValueOrDefault( metallicRoughness ),
        .pbr_specular_glossiness     = {},
        .clearcoat                   = {},
        .ior                         = {},
        .specular                    = {},
        .sheen                       = {},
        .transmission                = {},
        .volume                      = {},
        .emissive_strength           = {},
        .iridescence                 = {},
        .normal_texture              = { .texture = normal, .texcoord = 0 },
        .occlusion_texture           = { .texture = nullptr, .texcoord = 0 },
        .emissive_texture            = { .texture = nullptr, .texcoord = 0 },
        .emissive_factor             = { rgprim.Emissive(), rgprim.Emissive(), rgprim.Emissive() },
        .alpha_mode                  = rgprim.AlphaMode(),
        .alpha_cutoff                = 0.5f,
        .double_sided                = false,
        .unlit                       = false,
        .extras                      = { .data = nullptr },
    };
}
}



RTGL1::GltfExporter::GltfExporter( const RgTransform& _worldTransform )
    : worldTransform( _worldTransform )
{
}

namespace
{

struct BinFile
{
    explicit BinFile( const std::filesystem::path& gltfPath )
        : uri( GetGltfBinURI( gltfPath ) )
        , file( GetGltfBinPath( gltfPath ), std::ios::out | std::ios::trunc | std::ios::binary )
        , fileOffset( 0 )
        , storage{}
    {
        assert( file );
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
            .normalized     = false,
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
            .normalized     = false,
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


#define copy_ranges swap_ranges


struct BeginCount
{
    ptrdiff_t offset;
    size_t    count;

    template< typename T >
    std::span< T > ToSpan( std::vector< T >& container ) const
    {
        return { container.begin() + offset, count };
    }

    template< typename T >
    T* ToPointer( std::vector< T >& container ) const
    {
        assert( count == 1 );
        return &container[ offset ];
    }

    template< typename T >
    std::vector< T* > ToVectorOfPointers( std::vector< T >& container ) const
    {
        std::vector< T* > ptrs;

        std::ranges::transform(
            ToSpan< T >( container ), std::back_inserter( ptrs ), ConvertRefToPtr< T > );

        return ptrs;
    }
};

template< typename T >
auto append_n( std::vector< T >& v, size_t toadd )
{
    auto begin = ptrdiff_t( v.size() );
    v.resize( v.size() + toadd );

    return BeginCount{ begin, toadd };
}


// Corresponds to RgMeshInfo
struct GltfRoot
{
    std::string name;
    RgTransform transform;

    cgltf_node* thisNode;

    std::span< cgltf_buffer_view > bufferViews;
    std::span< cgltf_accessor >    accessors;
    std::span< cgltf_attribute >   attributes;
    std::span< cgltf_primitive >   primitives;
    std::span< cgltf_material >    materials;
    cgltf_mesh*                    mesh;

    std::vector< std::shared_ptr< RTGL1::DeepCopyOfPrimitive > > source;
};

struct GltfStorage
{
    explicit GltfStorage(
        const rgl::unordered_map< RTGL1::GltfMeshNode,
                                  std::vector< std::shared_ptr< RTGL1::DeepCopyOfPrimitive > > >&
            scene )
    {
        struct Ranges
        {
            BeginCount bufferViews;
            BeginCount accessors;
            BeginCount attributes;
            BeginCount primitives;
            BeginCount materials;
            BeginCount mesh;
            BeginCount thisNode;
        };
        std::queue< Ranges > ranges;

        // alloc
        for( const auto& [ meshNode, prims ] : scene )
        {
            size_t primsPerMesh = prims.size();

            ranges.push( Ranges{
                .bufferViews = append_n( allBufferViews, primsPerMesh * BufferViewsPerPrim ),
                .accessors   = append_n( allAccessors, primsPerMesh * AccessorsPerPrim ),
                .attributes  = append_n( allAttributes, primsPerMesh * AttributesPerPrim ),
                .primitives  = append_n( allPrimitives, primsPerMesh ),
                .materials   = append_n( allMaterials, primsPerMesh ),
                .mesh        = append_n( allMeshes, 1 ),
                .thisNode    = append_n( allNodes, 1 ),
            } );
        }
        BeginCount worldbc = append_n( allNodes, 1 );

        // resolve pointers
        for( const auto& [ meshNode, prims ] : scene )
        {
            const Ranges r = ranges.front();
            ranges.pop();

            GltfRoot root = {
                .name        = meshNode.name,
                .transform   = meshNode.transform,
                .thisNode    = r.thisNode.ToPointer( allNodes ),
                .bufferViews = r.bufferViews.ToSpan( allBufferViews ),
                .accessors   = r.accessors.ToSpan( allAccessors ),
                .attributes  = r.attributes.ToSpan( allAttributes ),
                .primitives  = r.primitives.ToSpan( allPrimitives ),
                .materials   = r.primitives.ToSpan( allMaterials ),
                .mesh        = r.mesh.ToPointer( allMeshes ),
                .source      = prims,
            };
            assert( root.source.size() == root.primitives.size() );

            worldRoots.push_back( root.thisNode );
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

    std::vector< cgltf_sampler >  allSamplers;
    std::vector< cgltf_image >    allImages;
    std::vector< cgltf_texture >  allTextures;
    std::vector< cgltf_material > allMaterials;

    std::vector< GltfRoot > roots; // each corresponds to RgMeshInfo

    cgltf_node*                world;
    std::vector< cgltf_node* > worldRoots;
};


bool PrepareFolder( const std::filesystem::path& folder )
{
    using namespace RTGL1;

    auto createEmptyFoldersFor = []( const std::filesystem::path& to ) {
        std::error_code ec;
        std::filesystem::create_directories( to, ec );
        if( ec )
        {
            debug::Warning(
                "{}: std::filesystem::create_directories error: {}", to.string(), ec.message() );
            return false;
        }
        assert( std::filesystem::is_directory( to ) );
        return true;
    };

    if( !std::filesystem::exists( folder ) )
    {
        return createEmptyFoldersFor( folder );
    }

#ifdef _WIN32
    {
        auto msg = std::format( "Folder already exists:\n{}\n\n"
                                "Are you sure you want to PERMANENTLY delete all its contents?",
                                std::filesystem::absolute( folder ).string() );

        int msgboxID = MessageBox(
            nullptr, msg.c_str(), "Overwrite folder", MB_ICONSTOP | MB_YESNO | MB_DEFBUTTON2 );

        if( msgboxID != IDYES )
        {
            return false;
        }
    }
    {
        std::error_code ec;

        auto count = std::filesystem::remove_all( folder, ec );
        if( ec )
        {
            debug::Warning(
                "{}: std::filesystem::remove_all error: {}", folder.string(), ec.message() );
            return false;
        }

        debug::Verbose( "{}: Removed {} files / directories", folder.string(), count );
    }

    return createEmptyFoldersFor( folder );
#else
    debug::Warning( "{}: Folder already exists, overwrite disabled", folder.string() );
    return false;
#endif // _WIN32
}

}

void RTGL1::GltfExporter::AddPrimitive( const RgMeshInfo&          mesh,
                                        const RgMeshPrimitiveInfo& primitive )
{
    if( !mesh.isExportable )
    {
        return;
    }

    if( Utils::IsCstrEmpty( mesh.pMeshName ) ||
        Utils::IsCstrEmpty( primitive.pPrimitiveNameInMesh ) )
    {
        debug::Warning( "Exporter requires mesh primitives to have pMeshName and "
                        "pPrimitiveNameInMesh specified. Ignoring primitive with ID: {} - {}",
                        mesh.uniqueObjectID,
                        primitive.primitiveIndexInMesh );
        return;
    }

    if( primitive.indexCount == 0 || primitive.pIndices == nullptr )
    {
        debug::Warning( "Exporter doesn't support primitives without index buffer: "
                        "{} - {} (with ID: {} - {})",
                        mesh.pMeshName,
                        primitive.pPrimitiveNameInMesh,
                        mesh.uniqueObjectID,
                        primitive.primitiveIndexInMesh );
        return;
    }

    scene[ GltfMeshNode{
               mesh.pMeshName,
               mesh.transform,
           } ]
        .emplace_back( std::make_shared< DeepCopyOfPrimitive >( primitive ) );
}

void RTGL1::GltfExporter::ExportToFiles( const std::filesystem::path& gltfPath )
{
    if( scene.empty() )
    {
        debug::Warning( "Nothing to export. Check uploaded primitives window" );
        return;
    }

    if( gltfPath.empty() )
    {
        debug::Warning( "Can't export: Destination path is empty" );
        return;
    }

    if( !PrepareFolder( GetGltfFolder( gltfPath ) ) )
    {
        debug::Warning( "Denied to write to the folder {}",
                        std::filesystem::absolute( GetGltfFolder( gltfPath ) ).string() );
        return;
    }


    const char* primExtrasExample  = nullptr; // "{ portalOutPosition\" : [0,0,0] }";
    const char* sceneExtrasExample = nullptr; // "{ tonemapping_enable\" : 1 }";


    // lock pointers
    BinFile     fbin( gltfPath );
    GltfStorage storage( scene );


    // for each RgMesh
    for( GltfRoot& root : storage.roots )
    {
        // for each RgMeshPrimitive
        for( size_t i = 0; i < root.source.size(); i++ )
        {
            const DeepCopyOfPrimitive& rgprim = *root.source[ i ];

            std::span viewsDst( root.bufferViews.begin() + ptrdiff_t( BufferViewsPerPrim * i ),
                                BufferViewsPerPrim );
            {
                std::ranges::move( MakeBufferViews( fbin, rgprim ), viewsDst.begin() );
            }

            std::span accessorsDst( root.accessors.begin() + ptrdiff_t( AccessorsPerPrim * i ),
                                    AccessorsPerPrim );
            {
                std::ranges::move(
                    MakeAccessors( rgprim.Vertices().size(), rgprim.Indices().size(), viewsDst ),
                    accessorsDst.begin() );
            }

            std::span vertAttrsDst( root.attributes.begin() + ptrdiff_t( AttributesPerPrim * i ),
                                    AttributesPerPrim );
            {
                std::ranges::move( MakeVertexAttributes( accessorsDst ), vertAttrsDst.begin() );
            }

            cgltf_texture* albedo     = nullptr;
            cgltf_texture* normal     = nullptr;
            cgltf_texture* metalrough = nullptr;
            {
            }

            root.materials[ i ] = MakeMaterial( rgprim, albedo, normal, metalrough );

            root.primitives[ i ] = cgltf_primitive{
                .type             = cgltf_primitive_type_triangles,
                .indices          = GetIndicesAccessor( accessorsDst ),
                .material         = &root.materials[ i ],
                .attributes       = std::data( vertAttrsDst ),
                .attributes_count = std::size( vertAttrsDst ),
                .extras           = {},
            };
        }

        *root.mesh = cgltf_mesh{
            .name             = const_cast< char* >( root.name.c_str() ),
            .primitives       = std::data( root.primitives ),
            .primitives_count = std::size( root.primitives ),
            .extras           = {},
        };

        *root.thisNode = cgltf_node{
            .name                    = const_cast< char* >( root.name.c_str() ),
            .parent                  = nullptr, /* later */
            .children                = nullptr,
            .children_count          = 0,
            .mesh                    = root.mesh,
            .camera                  = nullptr,
            .light                   = nullptr,
            .has_matrix              = true,
            .matrix                  = RG_TRANSFORM_TO_GLTF_MATRIX( root.transform ),
            .extras                  = { .data = const_cast< char* >( primExtrasExample ) },
            .has_mesh_gpu_instancing = false,
            .mesh_gpu_instancing     = {},
        };
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
        .materials          = std::data( storage.allMaterials ),
        .materials_count    = std::size( storage.allMaterials ),
        .accessors          = std::data( storage.allAccessors ),
        .accessors_count    = std::size( storage.allAccessors ),
        .buffer_views       = std::data( storage.allBufferViews ),
        .buffer_views_count = std::size( storage.allBufferViews ),
        .buffers            = fbin.Get(),
        .buffers_count      = 1,
        .images             = std::data( storage.allImages ),
        .images_count       = std::size( storage.allImages ),
        .textures           = std::data( storage.allTextures ),
        .textures_count     = std::size( storage.allTextures ),
        .samplers           = std::data( storage.allSamplers ),
        .samplers_count     = std::size( storage.allSamplers ),
        .nodes              = std::data( storage.allNodes ),
        .nodes_count        = std::size( storage.allNodes ),
        .scenes             = &gltfScene,
        .scenes_count       = 1,
        .scene              = &gltfScene,
    };

    cgltf_options options = {};
    cgltf_result  r;

    r = cgltf_validate( &data );
    if( r != cgltf_result_success )
    {
        debug::Warning( "cgltf_validate fail" );
        return;
    }

    r = cgltf_write_file( &options, gltfPath.string().c_str(), &data );
    if( r != cgltf_result_success )
    {
        debug::Warning( "cgltf_write_file fail" );
        return;
    }

    debug::Info( "{}: Exported successfully",
                 std::filesystem::absolute( GetGltfFolder( gltfPath ) ).string() );
}

bool RTGL1::GltfMeshNode::operator==( const GltfMeshNode& other ) const
{
    return this->name == other.name && TransformsAreEqual( this->transform, other.transform );
}

bool RTGL1::GltfMeshNode::operator<( const GltfMeshNode& other ) const
{
    return this->name < other.name && TransformIsLess( this->transform, other.transform );
}

uint64_t RTGL1::GltfMeshNode::Hash() const
{
    uint64_t h = 0;
    HashCombine( h, name );
    HashCombine( h, transform );

    return h;
}
