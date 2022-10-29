#include <chrono>
#include <iostream>
#include <cstring>
#include <span>


#ifdef _WIN32
    #define RG_USE_SURFACE_WIN32
#else
    #define RG_USE_SURFACE_XLIB
#endif
#include <RTGL1/RTGL1.h>

#define RG_CHECK( x )                                                              \
    assert( ( x ) == RG_RESULT_SUCCESS || ( x ) == RG_RESULT_SUCCESS_FOUND_MESH || \
            ( x ) == RG_RESULT_SUCCESS_FOUND_TEXTURE )


#pragma region BOILERPLATE

#include <GLFW/glfw3.h>
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
#else
    #define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Libs/tinygltf/tiny_gltf.h"


#ifndef ASSET_DIRECTORY
    #define ASSET_DIRECTORY ""
#endif


namespace
{
GLFWwindow* g_GlfwHandle;

glm::vec3 ctl_CameraPosition  = glm::vec3( 0, 0, -5 );
glm::vec3 ctl_CameraDirection = glm::vec3( 0, 0, -1 );
glm::vec3 ctl_LightPosition   = glm::vec3( 0, 0, 1 );
float     ctl_LightIntensity  = 1.0f;
float     ctl_LightCount      = 0.0f;
float     ctl_SunIntensity    = 10.0f;
float     ctl_SkyIntensity    = 0.2f;
RgBool32  ctl_SkyboxEnable    = 1;
float     ctl_Roughness       = 0.05f;
float     ctl_Metallicity     = 1.0f;
RgBool32  ctl_MoveBoxes       = 0;
RgBool32  ctl_ShowGradients   = 0;
RgBool32  ctl_ReloadShaders   = 0;

bool ProcessWindow()
{
    if( glfwWindowShouldClose( g_GlfwHandle ) ) return false;
    glfwPollEvents(); return true;
}

void ProcessInput()
{
    static auto IsPressed    = []( int key ) { return glfwGetKey( g_GlfwHandle, ( key ) ) == GLFW_PRESS; };
    static auto ControlFloat = []( int key, float& value, float speed, float minval = 0.0f, float maxval = 1.0f ) {
            if( IsPressed( key ) ) {
                if( IsPressed( GLFW_KEY_KP_ADD ) ) value += speed;
                if( IsPressed( GLFW_KEY_KP_SUBTRACT ) ) value -= speed; }
            value = std::clamp( value, minval, maxval ); };
    static auto lastTimePressed = std::chrono::system_clock::now();
    static auto ControlSwitch   = []( int key, uint32_t& value, uint32_t stateCount = 2 ) {
        if( IsPressed( key ) ) {
            float secondsSinceLastTime = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now() - lastTimePressed ).count() / 1000.0f;
            if( secondsSinceLastTime < 0.5f ) return;
            value           = ( value + 1 ) % stateCount;
            lastTimePressed = std::chrono::system_clock::now(); } };

    float           cameraSpeed = 5.0f;
    float           delta       = 1.0 / 60.0f;
    const glm::vec3 d           = ctl_CameraDirection;
    const glm::vec3 u           = glm::vec3( 0, 1, 0 );
    const glm::vec3 r           = glm::cross( d, u );

    if( IsPressed( GLFW_KEY_W ) )       ctl_CameraPosition += d * delta * cameraSpeed;
    if( IsPressed( GLFW_KEY_S ) )       ctl_CameraPosition -= d * delta * cameraSpeed;
    if( IsPressed( GLFW_KEY_D ) )       ctl_CameraPosition += r * delta * cameraSpeed;
    if( IsPressed( GLFW_KEY_A ) )       ctl_CameraPosition -= r * delta * cameraSpeed;
    if( IsPressed( GLFW_KEY_E ) )       ctl_CameraPosition += u * delta * cameraSpeed;
    if( IsPressed( GLFW_KEY_Q ) )       ctl_CameraPosition -= u * delta * cameraSpeed;

    if( IsPressed( GLFW_KEY_LEFT ) )    ctl_CameraDirection = glm::rotate( ctl_CameraDirection, delta * 2, glm::vec3( 0, 1, 0 ) );
    if( IsPressed( GLFW_KEY_RIGHT ) )   ctl_CameraDirection = glm::rotate( ctl_CameraDirection, -delta * 2, glm::vec3( 0, 1, 0 ) );

    if( IsPressed( GLFW_KEY_KP_8 ) )    ctl_LightPosition[ 2 ] += delta * 5;
    if( IsPressed( GLFW_KEY_KP_5 ) )    ctl_LightPosition[ 2 ] -= delta * 5;
    if( IsPressed( GLFW_KEY_KP_6 ) )    ctl_LightPosition[ 0 ] += delta * 5;
    if( IsPressed( GLFW_KEY_KP_4 ) )    ctl_LightPosition[ 0 ] -= delta * 5;
    if( IsPressed( GLFW_KEY_KP_9 ) )    ctl_LightPosition[ 1 ] += delta * 5;
    if( IsPressed( GLFW_KEY_KP_7 ) )    ctl_LightPosition[ 1 ] -= delta * 5;

    ControlFloat( GLFW_KEY_R, ctl_Roughness,        delta,      0, 1 );
    ControlFloat( GLFW_KEY_M, ctl_Metallicity,      delta,      0, 1 );
    ControlFloat( GLFW_KEY_Y, ctl_LightIntensity,   delta,      0, 1000 );
    ControlFloat( GLFW_KEY_Y, ctl_LightCount,       delta * 5,  0, 1000 );
    ControlFloat( GLFW_KEY_I, ctl_SunIntensity,     delta,      0, 1000 );
    ControlFloat( GLFW_KEY_O, ctl_SkyIntensity,     delta,      0, 1000 );

    ctl_ReloadShaders = false;
    ControlSwitch( GLFW_KEY_TAB,        ctl_SkyboxEnable );
    ControlSwitch( GLFW_KEY_Z,          ctl_MoveBoxes );
    ControlSwitch( GLFW_KEY_G,          ctl_ShowGradients );
    ControlSwitch( GLFW_KEY_BACKSLASH,  ctl_ReloadShaders );
}

double GetCurrentTimeInSeconds()
{
    static auto timeStart = std::chrono::system_clock::now();
    return double( std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now() - timeStart ).count() ) / 1000.0;
}

const RgFloat3D s_CubePositions[] = { 
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, 
    { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, 
    { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, 
    {-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, 
    {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, 
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, 
};
const RgFloat2D s_CubeTexCoords[] = {
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
};
const RgPrimitiveVertex* GetCubeVertices()
{
    static RgPrimitiveVertex verts[ std::size( s_CubePositions ) ] = {};
    for( size_t i = 0; i < std::size( s_CubePositions ); i++ )
    {
        memcpy( verts[ i ].position, &s_CubePositions[ i ], 3 * sizeof( float ) );
        memcpy( verts[ i ].texCoord, &s_CubeTexCoords[ i ], 2 * sizeof( float ) );
        verts[ i ].color = rgUtilPackColorByte4D( 255, 255, 255, 255 );
    }
    return verts;
}

const RgFloat3D s_QuadPositions[] = {
    {0,0,0}, {0,1,0}, {1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
};
const RgFloat2D s_QuadTexCoords[] = {
    {0,0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {1, 1},
};
const uint32_t s_QuadColorsABGR[] = {
    0xF0FF0000, 0xF0FFFFFF, 0xF0FFFFFF, 0xF0FFFFFF, 0xFFFFFFFF, 0xFF00FF00,
};
const RgPrimitiveVertex* GetQuadVertices()
{
    static RgPrimitiveVertex verts[ std::size( s_QuadPositions ) ] = {};
    for( size_t i = 0; i < std::size( s_QuadPositions ); i++ )
    {
        memcpy( verts[ i ].position, &s_QuadPositions[ i ], 3 * sizeof( float ) );
        memcpy( verts[ i ].texCoord, &s_QuadTexCoords[ i ], 2 * sizeof( float ) );
        verts[ i ].color = s_QuadColorsABGR[ i ];
    }
    return verts;
}

uint32_t MurmurHash32( std::string_view str, uint32_t seed = 0 )
{
    const uint32_t m   = 0x5bd1e995;
    const uint32_t r   = 24;

    uint32_t       len = uint32_t( str.length() );
    uint32_t       h   = seed ^ len;
    auto*          data = reinterpret_cast< const uint8_t* >( str.data() );

    while( len >= 4 )
    {
        unsigned int k = *reinterpret_cast< const uint32_t* >( data );

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    if( len == 3 )
    {
        h ^= data[ 2 ] << 16;
    }

    if( len == 2 )
    {
        h ^= data[ 1 ] << 8;
    }

    if( len == 1 )
    {
        h ^= data[ 0 ];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

using MeshName = std::string;
struct WorldMeshPrimitive
{
    RgTransform                      transform;
    std::vector< RgPrimitiveVertex > vertices;
    std::vector< uint32_t >          indices;
    std::string                      texture;
};
std::unordered_map< MeshName, std::vector< WorldMeshPrimitive > > g_allMeshes;

void ForEachGltfMesh( const tinygltf::Model& model, const tinygltf::Node& node )
{
    if( node.mesh >= 0 && node.mesh < static_cast< int >( model.meshes.size() ) )
    {
        std::string_view meshName = model.meshes[ node.mesh ].name;

        for( const auto& primitive : model.meshes[ node.mesh ].primitives )
        {
            std::vector< RgPrimitiveVertex > rgverts;
            std::vector< uint32_t > rgindices;

            for( const auto& [ attribName, accessId ] : primitive.attributes )
            {
                auto& attribAccessor = model.accessors[ accessId ];
                auto& attribView     = model.bufferViews[ attribAccessor.bufferView ];
                auto& attribBuffer   = model.buffers[ attribView.buffer ];

                const uint8_t* data =
                    &attribBuffer.data[ attribAccessor.byteOffset + attribView.byteOffset ];
                int dataStride = attribAccessor.ByteStride( attribView );

                if( rgverts.empty() )
                {
                    rgverts.resize( attribAccessor.count );
                }
                assert( rgverts.size() == attribAccessor.count );

                std::tuple< const char*, size_t, size_t > attr[] = {
                    { "POSITION", offsetof( RgPrimitiveVertex, position ), sizeof( float ) * 3 },
                    { "NORMAL", offsetof( RgPrimitiveVertex, normal ), sizeof( float ) * 3 },
                    { "TEXCOORD_0", offsetof( RgPrimitiveVertex, texCoord ), sizeof( float ) * 2 },
                };

                for( const auto& [ name, fieldOffset, elemSize ] : attr )
                {
                    if( attribName == name )
                    {
                        for( uint64_t i = 0; i < rgverts.size(); i++ )
                        {
                            const uint8_t* src = data + i * dataStride;
                            auto*          dst = reinterpret_cast< uint8_t* >( &rgverts[ i ] );

                            memcpy( dst + fieldOffset, src, elemSize );
                        }
                    }
                }
            }

            for( auto& v : rgverts )
            {
                v.color = 0xFFFFFFFF;
            }

            {
                auto& indexAccessor = model.accessors[ primitive.indices ];
                auto& indexView     = model.bufferViews[ indexAccessor.bufferView ];
                auto& indexBuffer   = model.buffers[ indexView.buffer ];

                const uint8_t* data = &indexBuffer.data[ indexAccessor.byteOffset + indexView.byteOffset ];
                int dataStride = indexAccessor.ByteStride( indexView );

                rgindices.resize( indexAccessor.count );

                for( uint64_t i = 0; i < rgindices.size(); i++ )
                {
                    uint32_t index = 0;

                    if( dataStride == sizeof( uint32_t ) )
                    {
                        index = *reinterpret_cast< const uint32_t* >( data + i * dataStride );
                    }
                    else if( dataStride == sizeof( uint16_t ) )
                    {
                        index = *reinterpret_cast< const uint16_t* >( data + i * dataStride );
                    }
                    else
                    {
                        assert( false );
                    }

                    rgindices[ i ] = index;
                }
            }

            auto translation = node.translation.size() == 3 ? glm::make_vec3( node.translation.data() )
                                                            : glm::dvec3( 0.0 );
            auto rotation    = node.rotation.size() == 4    ? glm::make_quat( node.rotation.data() )
                                                            : glm::dmat4( 1.0 );
            auto scale       = node.scale.size() == 3       ? glm::make_vec3( node.scale.data() )
                                                            : glm::dvec3( 1.0 );
            glm::dmat4 dtr;
            if( node.matrix.size() == 16 )
            {
                dtr = glm::make_mat4x4( node.matrix.data() );
            }
            else
            {
                dtr = glm::translate( glm::dmat4( 1.0 ), translation ) * 
                      glm::dmat4( rotation ) *
                      glm::scale( glm::dmat4( 1.0 ), scale );
            }
            auto tr = glm::mat4( dtr );

            RgTransform rgtransform = { {
                { tr[ 0 ][ 0 ], tr[ 1 ][ 0 ], tr[ 2 ][ 0 ], tr[ 3 ][ 0 ] },
                { tr[ 0 ][ 1 ], tr[ 1 ][ 1 ], tr[ 2 ][ 1 ], tr[ 3 ][ 1 ] },
                { tr[ 0 ][ 2 ], tr[ 1 ][ 2 ], tr[ 2 ][ 2 ], tr[ 3 ][ 2 ] },
            } };

            std::string texName;
            {
                int tex = model.materials[ primitive.material ]
                              .pbrMetallicRoughness.baseColorTexture.index;
                if( tex >= 0 && model.textures[ tex ].source >= 0 )
                {
                    auto& image = model.images[ model.textures[ tex ].source ];
                    texName     = image.uri;
                }
            }

            g_allMeshes[ meshName.data() ].push_back( WorldMeshPrimitive{
                .transform = rgtransform,
                .vertices  = std::move( rgverts ),
                .indices   = std::move( rgindices ),
                .texture   = std::move( texName ),
            } );
        }
    }

    for( int c : node.children )
    {
        assert( c >= 0 && c < static_cast< int >( model.nodes.size() ) );
        ForEachGltfMesh( model, model.nodes[ c ] );
    }
}

void FillGAllMeshes( std::string_view path,
                     const std::function< void( const char* pTextureName, const void* pPixels, uint32_t w, uint32_t h ) >& materialFunc )
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;
    if( loader.LoadASCIIFromFile( &model, &err, &warn, path.data() ) )
    {
        for( uint64_t m = 0; m < model.materials.size(); m++ )
        {
            const auto& gltfMat = model.materials[ m ];

            int itextures[] = {
                gltfMat.pbrMetallicRoughness.baseColorTexture.index,
                gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index,
                gltfMat.normalTexture.index,
            };

            for( int tex : itextures )
            {
                if( tex >= 0 && model.textures[ tex ].source >= 0 )
                {
                    auto& image = model.images[ model.textures[ tex ].source ];
                    assert( image.bits == 8 );

                    materialFunc( image.uri.c_str(),
                                  &image.image[ 0 ],
                                  uint32_t( image.width ),
                                  uint32_t( image.height ) );
                }
            }
        }

        const auto& scene = model.scenes[ model.defaultScene ];
        for( int sceneNode : scene.nodes )
        {
            ForEachGltfMesh( model, model.nodes[ sceneNode ] );
        }
    }
    else
    {
        std::cout << "Can't load GLTF. " << err << std::endl << warn << std::endl;
    }
}
}
#pragma endregion BOILERPLATE





void MainLoop( RgInstance instance, std::string_view gltfPath )
{
    RgResult  r       = RG_RESULT_SUCCESS;
    uint64_t  frameId = 0;


    // some resources can be initialized out of frame
    {
        const uint32_t        white      = 0xFFFFFFFF;
        RgOriginalCubemapInfo skyboxInfo = 
        {
            .pTextureName     = "Cubemap/",
            .pPixelsPositiveX = &white,
            .pPixelsNegativeX = &white,
            .pPixelsPositiveY = &white,
            .pPixelsNegativeY = &white,
            .pPixelsPositiveZ = &white,
            .pPixelsNegativeZ = &white,
            .sideSize = 1,
        };
        r = rgProvideOriginalCubemapTexture( instance, &skyboxInfo );
        RG_CHECK( r );

        
        auto uploadMaterial = [ instance ]( const char* pTextureName,
                                            const void* pPixels,
                                            uint32_t    w,
                                            uint32_t    h ) {
            RgOriginalTextureInfo info =
            {
                .pTextureName = pTextureName,
                .pPixels = pPixels,
                .size = { w, h },
            };
            RgResult t = rgProvideOriginalTexture( instance, &info );
            RG_CHECK( t );
        };

        uint32_t indexInMesh = 0;

        RgMeshInfo meshInfo = {
            .uniqueObjectID = 0,
            .pMeshName      = gltfPath.data(),
            .isStatic       = true,
            .animationName  = nullptr,
            .animationTime  = 0.0f,
        };

        /* g_allMeshes = */ FillGAllMeshes( gltfPath, uploadMaterial );
    }


    while( ProcessWindow() )
    {
        ProcessInput();

        {
            RgStartFrameInfo startInfo = {
                .requestVSync        = true,
                .requestShaderReload = ctl_ReloadShaders,
            };
            r = rgStartFrame( instance, &startInfo );
            RG_CHECK( r );
        }


        for( const auto& [ meshName, primitives ] : g_allMeshes )
        {
            std::string objectName = "obj_" + meshName;

            RgMeshInfo  mesh = {
                 .uniqueObjectID = MurmurHash32( objectName ),
                 .pMeshName      = meshName.c_str(),
                 .isStatic       = false,
                 .animationName  = nullptr,
                 .animationTime  = 0.0f,
            };

            uint32_t index = 0; 
            for( const auto& srcPrim : primitives )
            {
                RgMeshPrimitiveInfo prim = {
                    .primitiveIndexInMesh = index++,
                    .flags                = 0,
                    .transform            = srcPrim.transform,
                    .pVertices            = srcPrim.vertices.data(),
                    .vertexCount          = uint32_t( srcPrim.vertices.size() ),
                    .pIndices             = srcPrim.indices.data(),
                    .indexCount           = uint32_t( srcPrim.indices.size() ),
                    .pTextureName         = srcPrim.texture.c_str(),
                    .textureFrame         = 0,
                    .color                = 0xFFFFFFFF,
                    .pEditorInfo          = nullptr,
                };

                r = rgUploadMeshPrimitive( instance, &mesh, &prim );
                RG_CHECK( r );
            }
        }


        {
            RgMeshInfo mesh = {
                .uniqueObjectID = 10,
                .pMeshName      = "test",
                .isStatic       = false,
                .animationName  = nullptr,
                .animationTime  = 0.0f,
            };

            RgMeshPrimitiveInfo prim = {
                .primitiveIndexInMesh = 0,
                .flags                = 0,
                .transform            = { {
                    { 1, 0, 0, ctl_MoveBoxes ? 5.0f - 0.05f * float( ( frameId + 30 ) % 200 ) : 1.0f },
                    { 0, 1, 0, 1.0f },
                    { 0, 0, 1, 0.0f },
                } },
                .pVertices            = GetCubeVertices(),
                .vertexCount          = std::size( s_CubePositions ),
                .pTextureName         = nullptr,
                .textureFrame         = 0,
                .color                = rgUtilPackColorByte4D( 128, 255, 128, 128 ),
                .pEditorInfo          = nullptr,
            };

            r = rgUploadMeshPrimitive( instance, &mesh, &prim );
            RG_CHECK( r );
        }


        // upload world-space rasterized geometry for non-expensive transparency
        {
            RgMeshInfo mesh = {
                .uniqueObjectID = 12,
                .pMeshName      = "test_raster",
                .isStatic       = false,
                .animationName  = nullptr,
                .animationTime  = 0.0f,
            };

            RgMeshPrimitiveInfo prim = {
                .primitiveIndexInMesh = 0,
                .flags                = 0,
                .transform            = { {
                    { 1, 0, 0, -0.5f },
                    { 0, 1, 0, 1.0f },
                    { 0, 0, 1, 1.0f },
                } },
                .pVertices            = GetQuadVertices(),
                .vertexCount          = std::size( s_QuadPositions ),
                .pTextureName         = nullptr,
                .textureFrame         = 0,
                // alpha is not 1.0
                .color                = rgUtilPackColorByte4D( 255, 128, 128, 128 ),
                .pEditorInfo          = nullptr,
            };

            r = rgUploadMeshPrimitive( instance, &mesh, &prim );
            RG_CHECK( r );
        }


        // set bounding box of the decal to modify G-buffer
        /*{
            RgDecalUploadInfo decalInfo = {
                .transform = { {
                    { 1, 0, 0, 0 },
                    { 0, 1, 0, 0 },
                    { 0, 0, 1, 0 },
                } },
                .material  = RG_NO_MATERIAL,
            };
            r = rgUploadDecal( instance, &decalInfo );
            RG_CHECK( r );
        }*/


        // upload the sun
        {
            RgDirectionalLightUploadInfo dirLight = {
                .color                  = { ctl_SunIntensity, ctl_SunIntensity, ctl_SunIntensity },
                .direction              = { -1, -8, -1 },
                .angularDiameterDegrees = 0.5f
            };
            r = rgUploadDirectionalLight( instance, &dirLight );
            RG_CHECK( r );
        }


        // upload sphere lights
        /*{
            uint32_t count = ( frameId % 2 ) * 64 + 128;

            for( uint64_t i = 0; i < count; i++ )
            {
                RgSphericalLightUploadInfo spherical = {
                    .uniqueID = i + 1,
                    .color    = { ctl_LightIntensity, ctl_LightIntensity, ctl_LightIntensity },
                    .position = { ctl_LightPosition[ 0 ] + i * 3,
                                  ctl_LightPosition[ 1 ],
                                  ctl_LightPosition[ 2 ] },
                    .radius   = 0.2f,
                };
                r = rgUploadSphericalLight( instance, &spherical );
                RG_CHECK( r );
            }
        }*/


        // submit the frame
        {
            RgDrawFrameSkyParams skyParams = {
                .skyType            = ctl_SkyboxEnable ? RG_SKY_TYPE_CUBEMAP : RG_SKY_TYPE_COLOR,
                .skyColorDefault    = { 0.71f, 0.88f, 1.0f },
                .skyColorMultiplier = ctl_SkyIntensity,
                .skyColorSaturation = 1.0f,
                .skyViewerPosition  = { 0, 0, 0 },
                .pSkyCubemapTextureName = "Cubemap/",
            };

            RgDrawFrameDebugParams debugParams = {
                .drawFlags = ctl_ShowGradients != 0 ? RG_DEBUG_DRAW_GRADIENTS_BIT : 0u,
            };

            RgDrawFrameRenderResolutionParams resolutionParams = {
                .upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
                .resolutionMode   = RG_RENDER_RESOLUTION_MODE_BALANCED,
            };

            RgPostEffectChromaticAberration chromaticAberration = {
                .isActive              = true,
                .intensity             = 0.3f,
            };

            glm::mat4 view = glm::lookAt(
                ctl_CameraPosition, ctl_CameraPosition + ctl_CameraDirection, { 0, 1, 0 } );

            RgDrawFrameInfo frameInfo =
            {
                .fovYRadians = glm::radians(75.0f),
                .cameraNear = 0.1f,
                .cameraFar = 10000.0f,
                .rayLength = 10000.0f,
                .rayCullMaskWorld = RG_DRAW_FRAME_RAY_CULL_WORLD_0_BIT,
                .currentTime = GetCurrentTimeInSeconds(),
                .pRenderResolutionParams = &resolutionParams,
                .pSkyParams = &skyParams,
                .pDebugParams = &debugParams,
                .postEffectParams = {
                    .pChromaticAberration = &chromaticAberration,
                },
            };
            // GLM is column major, copy matrix data directly
            memcpy( frameInfo.view, &view[ 0 ][ 0 ], 16 * sizeof( float ) );

            r = rgDrawFrame( instance, &frameInfo );
            RG_CHECK( r );
        }


        frameId++;
    }
}


int main( int argc, char* argv[] )
{
    glfwInit(); glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API ); glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );
    g_GlfwHandle = glfwCreateWindow( 1600, 900, "RTGL1 Test", nullptr, nullptr );


    RgResult   r;
    RgInstance instance;

#ifdef _WIN32
    RgWin32SurfaceCreateInfo win32Info = {
        .hinstance = GetModuleHandle( NULL ),
        .hwnd      = glfwGetWin32Window( g_GlfwHandle ),
    };
#else
    RgXlibSurfaceCreateInfo xlibInfo = {
        .dpy    = glfwGetX11Display(),
        .window = glfwGetX11Window( g_GlfwHandle ),
    };
#endif

    RgInstanceCreateInfo info = {
        .pAppName = "RTGL1 Test",
        .pAppGUID = "459d6734-62a6-4d47-927a-bedcdb0445c5",

#ifdef _WIN32
        .pWin32SurfaceInfo = &win32Info,
#else
        .pXlibSurfaceCreateInfo = &xlibInfo,
#endif

        .pfnPrint = []( const char* pMessage, RgMessageSeverityFlags severity, void *pUserData )
            {
                std::cout << pMessage << std::endl;
            },

        .pShaderFolderPath  = ASSET_DIRECTORY,
        .pBlueNoiseFilePath = ASSET_DIRECTORY "BlueNoise_LDR_RGBA_128.ktx2",

        .primaryRaysMaxAlbedoLayers          = 1,
        .indirectIlluminationMaxAlbedoLayers = 1,

        .rayCullBackFacingTriangles = false,

        .rasterizedMaxVertexCount = 4096,
        .rasterizedMaxIndexCount  = 2048,

        .rasterizedSkyCubemapSize = 256,

        .maxTextureCount                   = 1024,
        .pOverridenTexturesFolderPath      = ASSET_DIRECTORY,
        .overridenAlbedoAlphaTextureIsSRGB = true,
        .pWaterNormalTexturePath           = ASSET_DIRECTORY "WaterNormal_n.ktx2",

        // to match the GLTF standard
        .pbrTextureSwizzling = RG_TEXTURE_SWIZZLING_NULL_ROUGHNESS_METALLIC,
    };

    r = rgCreateInstance( &info, &instance );
    RG_CHECK( r );

    {
        auto gltfPath = argc > 1 ? argv[ 1 ] : ASSET_DIRECTORY "Sponza/glTF/Sponza.gltf";
        MainLoop( instance, gltfPath );
    }

    r = rgDestroyInstance( instance );
    RG_CHECK( r );


    glfwDestroyWindow( g_GlfwHandle );
    glfwTerminate();

    return 0;
}