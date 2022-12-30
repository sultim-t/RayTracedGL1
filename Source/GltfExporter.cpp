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
#include "SpanCounted.h"
#include "TextureExporter.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

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

std::filesystem::path GetOriginalTexturesFolder( const std::filesystem::path& gltfPath )
{
    return GetGltfFolder( gltfPath ) / RTGL1::TEXTURES_FOLDER_JUNCTION_PREFIX;
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
    explicit DeepCopyOfPrimitive( const RgMeshPrimitiveInfo& c )
        : info( c ), editor( c.pEditorInfo ? *c.pEditorInfo : RgEditorInfo{} )
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
        , editor( other.editor )
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
        this->info   = other.info;
        this->editor = other.editor;
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
    float     Emissive() const { return Utils::Saturate( info.emissive ); }

    float Roughness() const
    {
        return editor.pbrInfoExists ? Utils::Saturate( editor.pbrInfo.roughnessDefault ) : 1.0f;
    }
    float Metallic() const
    {
        return editor.pbrInfoExists ? Utils::Saturate( editor.pbrInfo.metallicDefault ) : 0.0f;
    }

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

        inout.info.pEditorInfo = &inout.editor;
    }

private:
    RgMeshPrimitiveInfo info;
    RgEditorInfo        editor;

    // to maintain lifetimes
    std::string                      pPrimitiveNameInMesh;
    std::string                      pTextureName;
    std::vector< RgPrimitiveVertex > pVertices;
    std::vector< uint32_t >          pIndices;
};
}



RTGL1::GltfExporter::GltfExporter( const RgTransform& _worldTransform )
    : worldTransform( _worldTransform )
{
}

namespace
{

struct GltfBin
{
    explicit GltfBin( const std::filesystem::path& gltfPath )
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


auto MakeBufferViews( GltfBin& fbin, const RTGL1::DeepCopyOfPrimitive& prim )
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
                                     GltfBin&,
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
    explicit GltfStorage( const RTGL1::MeshesToTheirPrimitives& scene, size_t lightCount )
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
        BeginCount lightsbc = append_n( allNodes, lightCount );
        BeginCount worldbc  = append_n( allNodes, 1 );

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

            worldChildren.push_back( root.thisNode );
            roots.push_back( std::move( root ) );
        }

        lightNodes = lightsbc.ToSpan( allNodes );
        for( cgltf_node& lightNode : lightNodes )
        {
            worldChildren.push_back( &lightNode );
        }

        world = worldbc.ToPointer( allNodes );

        assert( ranges.empty() );
        assert( worldChildren.size() == roots.size() + lightCount );
    }

    std::vector< cgltf_buffer_view > allBufferViews;
    std::vector< cgltf_accessor >    allAccessors;
    std::vector< cgltf_attribute >   allAttributes;
    std::vector< cgltf_primitive >   allPrimitives;
    std::vector< cgltf_material >    allMaterials;
    std::vector< cgltf_mesh >        allMeshes;
    std::vector< cgltf_node >        allNodes;

    std::vector< GltfRoot > roots; // each corresponds to RgMeshInfo

    cgltf_node*                world{ nullptr };
    std::vector< cgltf_node* > worldChildren;

    std::span< cgltf_node > lightNodes;
};


struct GltfTextures
{
    explicit GltfTextures( const std::set< std::string >& sceneMaterials,
                           const std::filesystem::path&   texturesFolder,
                           const RTGL1::TextureManager&   textureManager )
    {
        // alloc max and lock pointers
        allocStrings.resize( RTGL1::TEXTURES_PER_MATERIAL_COUNT * sceneMaterials.size() );
        allocImages.resize( RTGL1::TEXTURES_PER_MATERIAL_COUNT * sceneMaterials.size() );
        allocTextures.resize( RTGL1::TEXTURES_PER_MATERIAL_COUNT * sceneMaterials.size() );

        strings  = rgl::span_counted( std::span( allocStrings ) );
        images   = rgl::span_counted( std::span( allocImages ) );
        textures = rgl::span_counted( std::span( allocTextures ) );

        constexpr auto makeSampler = []( RgSamplerAddressMode addrU, RgSamplerAddressMode addrV ) {
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_sampler_wraps
            return cgltf_sampler{
                .name       = nullptr,
                .mag_filter = 0, // default
                .min_filter = 0, // default
                .wrap_s     = addrU == RG_SAMPLER_ADDRESS_MODE_CLAMP ? 33071 : 10497,
                .wrap_t     = addrV == RG_SAMPLER_ADDRESS_MODE_CLAMP ? 33071 : 10497,
            };
        };
        allocSamplers = {
            makeSampler( RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            makeSampler( RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_CLAMP ),
            makeSampler( RG_SAMPLER_ADDRESS_MODE_CLAMP, RG_SAMPLER_ADDRESS_MODE_REPEAT ),
            makeSampler( RG_SAMPLER_ADDRESS_MODE_CLAMP, RG_SAMPLER_ADDRESS_MODE_CLAMP ),
        };
        auto findSampler = [ this ]( const RTGL1::TextureManager::ExportResult& r ) {
            cgltf_sampler target = makeSampler( r.addressModeU, r.addressModeV );
            for( cgltf_sampler& found : allocSamplers )
            {
                if( std::memcmp( &found, &target, sizeof( cgltf_sampler ) ) == 0 )
                {
                    return &found;
                }
            }
            assert( 0 );
            return &allocSamplers[ 0 ];
        };


        // resolve
        for( const auto& materialName : sceneMaterials )
        {
            if( materialName.empty() )
            {
                continue;
            }

            static_assert( RTGL1::TEXTURES_PER_MATERIAL_COUNT == 4 );
            static_assert( RTGL1::TEXTURE_ALBEDO_ALPHA_INDEX == 0 );
            static_assert( RTGL1::TEXTURE_OCCLUSION_ROUGHNESS_METALLIC_INDEX == 1 );
            static_assert( RTGL1::TEXTURE_NORMAL_INDEX == 2 );
            static_assert( RTGL1::TEXTURE_EMISSIVE_INDEX == 3 );
            auto [ albedo, orm, normal, emissive ] = textureManager.ExportMaterialTextures(
                materialName.c_str(), texturesFolder, false );

            auto tryMakeCgltfTexture =
                [ this, &findSampler, &materialName ](
                    RTGL1::TextureManager::ExportResult&& r ) -> cgltf_texture* {
                if( !r.relativePath.empty() )
                {
                    std::string&   str = strings.increment_and_get();
                    cgltf_image&   img = images.increment_and_get();
                    cgltf_texture& txd = textures.increment_and_get();

                    // need to protect a string, to avoid dangling pointers
                    str = std::string( RTGL1::TEXTURES_FOLDER_JUNCTION_PREFIX ) + r.relativePath;
                    std::ranges::replace( str, '\\', '/' );

                    img = cgltf_image{
                        .name = const_cast< char* >( materialName.c_str() ),
                        .uri  = const_cast< char* >( str.c_str() ),
                    };

                    txd = cgltf_texture{
                        .image   = &img,
                        .sampler = findSampler( r ),
                    };

                    return &txd;
                }
                return nullptr;
            };

            materialAccess[ materialName ] = TextureSet{
                .albedo   = tryMakeCgltfTexture( std::move( albedo ) ),
                .orm      = tryMakeCgltfTexture( std::move( orm ) ),
                .normal   = tryMakeCgltfTexture( std::move( normal ) ),
                .emissive = tryMakeCgltfTexture( std::move( emissive ) ),
            };
        }
    }

    struct TextureSet
    {
        cgltf_texture* albedo{ nullptr };
        cgltf_texture* orm{ nullptr };
        cgltf_texture* normal{ nullptr };
        cgltf_texture* emissive{ nullptr };
    };

    TextureSet Access( std::string_view materialName ) const
    {
        auto iter = materialAccess.find( std::string( materialName ) );
        return iter != materialAccess.end() ? iter->second : TextureSet{};
    }

    auto Samplers() { return std::span( allocSamplers ); }
    auto Images() { return images.get_counted_subspan(); }
    auto Textures() { return textures.get_counted_subspan(); }

private:
    std::vector< std::string >   allocStrings;
    std::vector< cgltf_sampler > allocSamplers;
    std::vector< cgltf_image >   allocImages;
    std::vector< cgltf_texture > allocTextures;

    rgl::span_counted< std::string >   strings;
    rgl::span_counted< cgltf_image >   images;
    rgl::span_counted< cgltf_texture > textures;

    std::unordered_map< std::string, TextureSet > materialAccess;
};

cgltf_material MakeMaterial( const RTGL1::DeepCopyOfPrimitive& rgprim,
                             const GltfTextures&               textureStorage )
{
    auto txd = textureStorage.Access( rgprim.MaterialName() );

    cgltf_pbr_metallic_roughness metallicRoughness = {
        .base_color_texture         = { .texture = txd.albedo, .texcoord = 0 },
        .metallic_roughness_texture = { .texture = txd.orm, .texcoord = 0 },
        .base_color_factor          = { RG_ACCESS_VEC4( rgprim.Color().data ) },
        .metallic_factor            = rgprim.Metallic(),
        .roughness_factor           = rgprim.Roughness(),
    };

    // if there are PBR textures, set to RTGL1 defaults
    if( metallicRoughness.metallic_roughness_texture.texture &&
        metallicRoughness.metallic_roughness_texture.texture->image )
    {
        metallicRoughness.metallic_factor  = 0.0f;
        metallicRoughness.roughness_factor = 1.0f;
    }

    return cgltf_material{
        .name                        = nullptr,
        .has_pbr_metallic_roughness  = true,
        .has_pbr_specular_glossiness = false,
        .has_clearcoat               = false,
        .has_transmission            = false,
        .has_volume                  = false,
        .has_ior                     = false,
        .has_specular                = false,
        .has_sheen                   = false,
        .has_emissive_strength       = false,
        .has_iridescence             = false,
        .pbr_metallic_roughness      = metallicRoughness,
        .pbr_specular_glossiness     = {},
        .clearcoat                   = {},
        .ior                         = {},
        .specular                    = {},
        .sheen                       = {},
        .transmission                = {},
        .volume                      = {},
        .emissive_strength           = {},
        .iridescence                 = {},
        .normal_texture              = { .texture = txd.normal, .texcoord = 0 },
        .occlusion_texture           = { .texture = txd.orm, .texcoord = 0 },
        .emissive_texture            = { .texture = txd.emissive, .texcoord = 0 },
        .emissive_factor             = { rgprim.Emissive(), rgprim.Emissive(), rgprim.Emissive() },
        .alpha_mode                  = rgprim.AlphaMode(),
        .alpha_cutoff                = 0.5f,
        .double_sided                = false,
        .unlit                       = false,
        .extras                      = { .data = nullptr },
    };
}

RgFloat3D operator-( const RgFloat3D& c )
{
    return { -c.data[ 0 ], -c.data[ 1 ], -c.data[ 2 ] };
}

struct GltfLights
{
private:
    static cgltf_light MakeLight( const RgDirectionalLightUploadInfo& sun )
    {
        auto fcolor = RTGL1::Utils::UnpackColor4DPacked32< RgFloat3D >( sun.color );

        return cgltf_light{
            .name      = nullptr,
            .color     = { RG_ACCESS_VEC3( fcolor.data ) },
            .intensity = sun.intensity,
            .type      = cgltf_light_type_directional,
        };
    }

    static cgltf_light MakeLight( const RgSphericalLightUploadInfo& sph )
    {
        auto fcolor = RTGL1::Utils::UnpackColor4DPacked32< RgFloat3D >( sph.color );

        return cgltf_light{
            .name      = nullptr,
            .color     = { RG_ACCESS_VEC3( fcolor.data ) },
            .intensity = sph.intensity,
            .type      = cgltf_light_type_point,
        };
    }

    static cgltf_light MakeLight( const RgSpotLightUploadInfo& spot )
    {
        auto fcolor = RTGL1::Utils::UnpackColor4DPacked32< RgFloat3D >( spot.color );

        return cgltf_light{
            .name                  = nullptr,
            .color                 = { RG_ACCESS_VEC3( fcolor.data ) },
            .intensity             = spot.intensity,
            .type                  = cgltf_light_type_spot,
            .spot_inner_cone_angle = spot.angleInner,
            .spot_outer_cone_angle = spot.angleOuter,
        };
    }

#define POLYLIGHT_AS_SPHERE 1
    static cgltf_light MakeLight( const RgPolygonalLightUploadInfo& poly )
    {
#if POLYLIGHT_AS_SPHERE
        RTGL1::debug::Warning( "GLTF doesn't support poly lights, exporting as sphere" );
        
        auto fcolor = RTGL1::Utils::UnpackColor4DPacked32< RgFloat3D >( poly.color );

        float     area;
        RgFloat3D normal;
        if( !RTGL1::Utils::GetNormalAndArea( poly.positions, normal, area ) )
        {
            area = 1.0f;
        }

        return cgltf_light{
            .name      = nullptr,
            .color     = { RG_ACCESS_VEC3( fcolor.data ) },
            .intensity = poly.intensity * std::sqrt( area ),
            .type      = cgltf_light_type_directional,
        };
#endif
    }


    static RgTransform MakeTransform( const RgDirectionalLightUploadInfo& sun )
    {
        return RTGL1::Utils::MakeTransform( { 0, 0, 0 }, -sun.direction );
    }

    static RgTransform MakeTransform( const RgSphericalLightUploadInfo& sph )
    {
        return RTGL1::Utils::MakeTransform( sph.position, { 0, 0, 1 } );
    }

    static RgTransform MakeTransform( const RgSpotLightUploadInfo& spot )
    {
        return RTGL1::Utils::MakeTransform( { 0, 0, 0 }, -spot.direction );
    }

    static RgTransform MakeTransform( const RgPolygonalLightUploadInfo& poly )
    {
#if POLYLIGHT_AS_SPHERE
        RgFloat3D center;
        for( const auto& v : poly.positions )
        {
            center.data[ 0 ] += v.data[ 0 ];
            center.data[ 1 ] += v.data[ 1 ];
            center.data[ 2 ] += v.data[ 2 ];
        }
        center.data[ 0 ] /= 3.0f;
        center.data[ 1 ] /= 3.0f;
        center.data[ 2 ] /= 3.0f;

        float     area;
        RgFloat3D normal;
        if( RTGL1::Utils::GetNormalAndArea( poly.positions, normal, area ) )
        {
            center.data[ 0 ] += 0.1f * normal.data[ 0 ];
            center.data[ 1 ] += 0.1f * normal.data[ 1 ];
            center.data[ 2 ] += 0.1f * normal.data[ 2 ];
        }

        return RTGL1::Utils::MakeTransform( center, { 0, 0, 1 } );
#else
        return RG_TRANSFORM_IDENTITY;
#endif
    }

public:
    explicit GltfLights( const std::vector< RTGL1::GenericLight >& sceneLights,
                         std::span< cgltf_node >                   dstLightNodes )
    {
        assert( dstLightNodes.size() == sceneLights.size() );

        // lock pointers
        storage.resize( sceneLights.size() );

        for( size_t i = 0; i < sceneLights.size(); i++ )
        {
            storage[ i ] = std::visit( []( auto&& specific ) { return MakeLight( specific ); },
                                       sceneLights[ i ] );

            RgTransform lightTransform = std::visit(
                []( auto&& specific ) { return MakeTransform( specific ); }, sceneLights[ i ] );

            dstLightNodes[ i ] = cgltf_node{
                .name            = nullptr,
                .parent          = nullptr, /* later */
                .children        = nullptr,
                .children_count  = 0,
                .light           = &storage[ i ],
                .has_translation = false,
                .has_rotation    = false,
                .has_scale       = false,
                .has_matrix      = true,
                .matrix          = RG_TRANSFORM_TO_GLTF_MATRIX( lightTransform ),
                .extras          = {},
            };
        }
    }

    auto Lights() { return std::span( storage ); }

private:
    std::vector< cgltf_light > storage;
};


bool PrepareFolder( const std::filesystem::path& gltfPath )
{
    using namespace RTGL1;

    auto folder = GetGltfFolder( gltfPath );

    auto createEmptyFoldersFor = [ &folder, &gltfPath ]() {
        // create empty folder for .gltf
        std::error_code ec;

        std::filesystem::create_directories( folder, ec );
        if( ec )
        {
            debug::Warning( "{}: std::filesystem::create_directories error: {}",
                            folder.string(),
                            ec.message() );
            return false;
        }
        assert( std::filesystem::is_directory( folder ) );

        // create junction folder to store original textures
        auto junction = GetOriginalTexturesFolder( gltfPath );

        // but there's no privilege to create symlinks,
        // so create a folder that contains texture copies from ovrd/mat or ovrd/matdev
#if 1
        std::filesystem::create_directories( junction, ec );
        if( ec )
        {
            debug::Warning( "{}: std::filesystem::create_directories error: {}",
                            folder.string(),
                            ec.message() );
            return false;
        }
        assert( std::filesystem::is_directory( folder ) );
#else
        auto ovrdTextures = ovrdFolder / TEXTURES_FOLDER_DEV;

        std::filesystem::create_directory_symlink( ovrdTextures, junction, ec );
        if( ec )
        {
            debug::Warning( "{}: std::filesystem::create_directory_symlink error: {}",
                            junction.string(),
                            ec.message() );
            return false;
        }
        assert( std::filesystem::is_symlink( junction ) );
        assert( std::filesystem::equivalent( ovrdTextures, junction ) );
#endif

        return true;
    };

    if( !std::filesystem::exists( folder ) )
    {
        return createEmptyFoldersFor();
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

    return createEmptyFoldersFor();
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

    if( !Utils::IsCstrEmpty( primitive.pTextureName ) )
    {
        sceneMaterials.insert( primitive.pTextureName );
    }
}

void RTGL1::GltfExporter::AddLight( const GenericLightPtr& light )
{
    constexpr auto unbox = []( const GenericLightPtr& ptr ) {
        return std::visit( []( auto&& specific ) -> GenericLight { return *specific; }, ptr );
    };

    bool isExportable =
        std::visit( []( auto&& specific ) { return specific->isExportable; }, light );

    if( !isExportable )
    {
        return;
    }

    sceneLights.push_back( unbox( light ) );
}

void RTGL1::GltfExporter::ExportToFiles( const std::filesystem::path& gltfPath,
                                         const TextureManager&        textureManager )
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

    if( !PrepareFolder( gltfPath ) )
    {
        debug::Warning( "Denied to write to the folder {}",
                        std::filesystem::absolute( GetGltfFolder( gltfPath ) ).string() );
        return;
    }


    const char* primExtrasExample  = nullptr; // "{ portalOutPosition\" : [0,0,0] }";
    const char* sceneExtrasExample = nullptr; // "{ tonemapping_enable\" : 1 }";


    // lock pointers
    GltfBin      fbin( gltfPath );
    GltfStorage  storage( scene, sceneLights.size() );
    GltfTextures textureStorage(
        sceneMaterials, GetOriginalTexturesFolder( gltfPath ), textureManager );
    GltfLights lightStorage( sceneLights, storage.lightNodes );


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

            root.materials[ i ] = MakeMaterial( rgprim, textureStorage );

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
            .children       = std::data( storage.worldChildren ),
            .children_count = std::size( storage.worldChildren ),
            .has_matrix     = true,
            .matrix         = RG_TRANSFORM_TO_GLTF_MATRIX( worldTransform ),
            .extras         = { .data = nullptr },
        };
        for( cgltf_node* child : storage.worldChildren )
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
        .images             = std::data( textureStorage.Images() ),
        .images_count       = std::size( textureStorage.Images() ),
        .textures           = std::data( textureStorage.Textures() ),
        .textures_count     = std::size( textureStorage.Textures() ),
        .samplers           = std::data( textureStorage.Samplers() ),
        .samplers_count     = std::size( textureStorage.Samplers() ),
        .lights             = std::data( lightStorage.Lights() ),
        .lights_count       = std::size( lightStorage.Lights() ),
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
