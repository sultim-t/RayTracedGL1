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

#define RG_CHECK( x ) assert( ( x ) == RG_SUCCESS )


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


#define ASSET_DIRECTORY "../../"


static GLFWwindow* g_GlfwHandle;

static glm::vec3 ctl_CameraPosition  = glm::vec3( 0, 0, -5 );
static glm::vec3 ctl_CameraDirection = glm::vec3( 0, 0, -1 );
static glm::vec3 ctl_LightPosition   = glm::vec3( 0, 0, 1 );
static float     ctl_LightIntensity  = 1.0f;
static float     ctl_LightCount      = 0.0f;
static float     ctl_SunIntensity    = 10.0f;
static float     ctl_SkyIntensity    = 0.2f;
static RgBool32  ctl_SkyboxEnable    = 1;
static float     ctl_Roughness       = 0.05f;
static float     ctl_Metallicity     = 1.0f;
static RgBool32  ctl_MoveBoxes       = 0;
static RgBool32  ctl_ShowGradients   = 0;
static RgBool32  ctl_ReloadShaders   = 0;

static bool ProcessWindow()
{
    if( glfwWindowShouldClose( g_GlfwHandle ) ) return false;
    glfwPollEvents(); return true;
}

static void ProcessInput()
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

static double GetCurrentTimeInSeconds()
{
    static auto timeStart = std::chrono::system_clock::now();
    return std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now() - timeStart ).count() / 1000.0;
}

static uint32_t PackColorToUint32( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    return ( static_cast< uint32_t >( a ) << 24 ) | 
           ( static_cast< uint32_t >( b ) << 16 ) |
           ( static_cast< uint32_t >( g ) << 8  ) | 
           ( static_cast< uint32_t >( r )       );
}

static const RgFloat3D s_CubePositions[] = { 
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, 
    { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, 
    { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, 
    {-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, 
    {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, 
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, 
};
static const RgFloat2D s_CubeTexCoords[] = {
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
    {0,0}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0}, 
    {0,1}, {0,1}, {0,0}, {1,0}, {0,1}, {0,1}, 
};
static const RgVertex* GetCubeVertices()
{
    static RgVertex verts[ std::size( s_CubePositions ) ] = {};
    for( size_t i = 0; i < std::size( s_CubePositions ); i++ )
    {
        memcpy( verts[ i ].position, &s_CubePositions[ i ], 3 * sizeof( float ) );
        memcpy( verts[ i ].texCoord, &s_CubeTexCoords[ i ], 2 * sizeof( float ) );
        verts[ i ].packedColor = PackColorToUint32( 255, 255, 255, 255 );
    }
    return verts;
}

static const RgFloat3D s_QuadPositions[] = {
    {0,0,0}, {0,1,0}, {1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
};
static const RgFloat2D s_QuadTexCoords[] = {
    {0,0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {1, 1},
};
static const uint32_t s_QuadColorsABGR[] = {
    0xF0FF0000, 0xF0FFFFFF, 0xF0FFFFFF, 0xF0FFFFFF, 0xFFFFFFFF, 0xFF00FF00,
};
static const RgVertex* GetQuadVertices()
{
    static RgVertex verts[ std::size( s_QuadPositions ) ] = {};
    for( size_t i = 0; i < std::size( s_QuadPositions ); i++ )
    {
        memcpy( verts[ i ].position, &s_QuadPositions[ i ], 3 * sizeof( float ) );
        memcpy( verts[ i ].texCoord, &s_QuadTexCoords[ i ], 2 * sizeof( float ) );
        verts[ i ].packedColor = s_QuadColorsABGR[ i ];
    }
    return verts;
}

void ForEachGltfMesh( const std::function< void( std::span< RgVertex > verts, std::span< uint32_t > indices, RgMaterial material, RgTransform transform ) >& meshFunc,
                      const std::vector< RgMaterial >&                    rgmaterials,
                      const tinygltf::Model&                              model,
                      const tinygltf::Node&                               node )
{
    if( node.mesh >= 0 && node.mesh < static_cast< int >( model.meshes.size() ) )
    {
        for( const auto& primitive : model.meshes[ node.mesh ].primitives )
        {
            std::vector< RgVertex > rgverts;
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
                    { "POSITION", offsetof( RgVertex, position ), sizeof( float ) * 3 },
                    { "NORMAL", offsetof( RgVertex, normal ), sizeof( float ) * 3 },
                    { "TEXCOORD_0", offsetof( RgVertex, texCoord ), sizeof( float ) * 2 },
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
                v.packedColor = 0xFFFFFFFF;
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

            meshFunc( rgverts, rgindices, rgmaterials[ primitive.material ], rgtransform );
        }
    }

    for( int c : node.children )
    {
        assert( c >= 0 && c < static_cast< int >( model.nodes.size() ) );
        ForEachGltfMesh( meshFunc, rgmaterials, model, model.nodes[ c ] );
    }
}

void ForEachGltfMesh( std::string_view path,
                      const std::function< void( std::span< RgVertex > verts, std::span< uint32_t > indices, RgMaterial material, RgTransform transform ) >& meshFunc,
                      const std::function< RgMaterial( uint32_t w, uint32_t h, const void* albedo, const void* rme, const void* normal ) >& materialFunc )
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;
    if( loader.LoadASCIIFromFile( &model, &err, &warn, path.data() ) )
    {
        std::vector< RgMaterial > rgmaterials;
        rgmaterials.resize( model.materials.size() );

        for( uint64_t m = 0; m < model.materials.size(); m++ )
        {
            const auto& gltfMat = model.materials[ m ];

            int itextures[ 3 ] = {
                gltfMat.pbrMetallicRoughness.baseColorTexture.index,
                gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index,
                gltfMat.normalTexture.index,
            };

            struct RGBA
            {
                uint8_t data[ 4 ];
            };

            auto torgba = []( const std::vector< double >& flt ) {
                return RGBA{
                    static_cast< uint8_t >( std::clamp< int >( 0, static_cast< int >( flt[ 0 ] * 255.0 ), 255 ) ),
                    static_cast< uint8_t >( std::clamp< int >( 0, static_cast< int >( flt[ 1 ] * 255.0 ), 255 ) ),
                    static_cast< uint8_t >( std::clamp< int >( 0, static_cast< int >( flt[ 2 ] * 255.0 ), 255 ) ),
                    static_cast< uint8_t >( std::clamp< int >( 0, static_cast< int >( flt[ 3 ] * 255.0 ), 255 ) ),
                };
            };

            RGBA fallback[ 3 ] = {
                torgba( gltfMat.pbrMetallicRoughness.baseColorFactor ),
                torgba( { 0.0,
                          gltfMat.pbrMetallicRoughness.metallicFactor,
                          gltfMat.pbrMetallicRoughness.roughnessFactor,
                          0.0 } ),
                torgba( { 0.0, 1.0, 0.0, 0.0 } ),
            };

            struct MaterialDef
            {
                const void* data;
                uint32_t    w;
                uint32_t    h;
            } defs[ 3 ] = {};

            for( size_t i = 0; i < std::size( itextures ); i++ )
            {
                int tex = itextures[ i ];
                if( tex >= 0 && model.textures[ tex ].source >= 0 )
                {
                    auto& image = model.images[ model.textures[ tex ].source ];
                    assert( image.bits == 8 );

                    defs[ i ] = {
                        .data = &image.image[ 0 ],
                        .w    = static_cast< uint32_t >( image.width ),
                        .h    = static_cast< uint32_t >( image.height ),
                    };
                }
                else
                {
                    defs[ i ] = {
                        .data = fallback[ i ].data,
                        .w    = 1,
                        .h    = 1,
                    };
                }
            }

            // RTGL restriction: must be the same size
            if( defs[ 1 ].w != defs[ 0 ].w || defs[ 1 ].h != defs[ 0 ].h )
            {
                defs[ 1 ] = {};
            }
            if( defs[ 2 ].w != defs[ 0 ].w || defs[ 2 ].h != defs[ 0 ].h )
            {
                defs[ 2 ] = {};
            }

            if( defs[ 0 ].data || defs[ 1 ].data || defs[ 2 ].data )
            {
                rgmaterials[ m ] = materialFunc(
                    defs[ 0 ].w, defs[ 0 ].h, defs[ 0 ].data, defs[ 1 ].data, defs[ 2 ].data );
            }
        }

        const auto& scene = model.scenes[ model.defaultScene ];
        for( int sceneNode : scene.nodes )
        {
            ForEachGltfMesh( meshFunc, rgmaterials, model, model.nodes[ sceneNode ] );
        }
    }
    else
    {
        std::cout << "Can't load GLTF. " << err << std::endl << warn << std::endl;
    }
}
#pragma endregion BOILERPLATE





static void MainLoop( RgInstance instance, std::string_view gltfPath )
{
    RgResult  r       = RG_SUCCESS;
    uint64_t  frameId = 0;
    RgCubemap skybox  = RG_NO_MATERIAL;


    // each geometry must have a unique ID
    // for matching between frames
    constexpr uint64_t MovableGeomUniqueID  = 200;
    constexpr uint64_t DynamicGeomUniqueID  = 201;
    constexpr uint64_t GltfGeomUniqueIDBase = 500;


    // some resources can be initialized out of frame
    {
        RgCubemapCreateInfo skyboxInfo = 
        {
            .relativePathFaces = {
                "Cubemap/px", "Cubemap/nx",
                "Cubemap/py", "Cubemap/ny",
                "Cubemap/pz", "Cubemap/nz", 
            },
            .useMipmaps = true,
        };
        r = rgCreateCubemap( instance, &skyboxInfo, &skybox );
        RG_CHECK( r );


        // upload static geometry of the scene once
        r = rgBeginStaticGeometries( instance );
        RG_CHECK( r );
        {
            auto uploadMaterial = [ instance ]( uint32_t    w,
                                                uint32_t    h,
                                                const void* albedo,
                                                const void* rme,
                                                const void* normal ) {
                RgMaterial material;

                RgMaterialCreateInfo info =
                {
                    .size = { w, h },
                    .textures = {
                        .pDataAlbedoAlpha = albedo,
                        .pDataRoughnessMetallicEmission = rme,
                        .pDataNormal = normal,
                    },
                };
                RgResult t = rgCreateMaterial( instance, &info, &material );
                RG_CHECK( t );

                return material;
            };

            uint64_t idCounter = 0;

            auto uploadStaticGeometry = [ instance, &idCounter ]( std::span< RgVertex > verts,
                                                                  std::span< uint32_t > indices,
                                                                  RgMaterial            material,
                                                                  RgTransform           transform ) {
                RgGeometryUploadInfo info = {
                    .uniqueID           = GltfGeomUniqueIDBase + idCounter,
                    .flags              = 0,
                    .geomType           = RG_GEOMETRY_TYPE_STATIC,
                    .passThroughType    = RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE,
                    .vertexCount        = static_cast< uint32_t >( verts.size() ),
                    .pVertices          = verts.data(),
                    .indexCount         = static_cast< uint32_t >( indices.size() ),
                    .pIndices           = indices.data(),
                    .layerColors        = { { 1.0f, 1.0f, 1.0f, 1.0f } },
                    .defaultRoughness   = 1.0f,
                    .defaultMetallicity = 0.0f,
                    .geomMaterial       = { material },
                    .transform          = transform,
                };
                RgResult t = rgUploadGeometry( instance, &info );
                RG_CHECK( t );

                idCounter++;
            };

            ForEachGltfMesh( gltfPath, uploadStaticGeometry, uploadMaterial );
        }
        {
            RgGeometryUploadInfo movable = {
                .uniqueID           = MovableGeomUniqueID,
                .flags              = RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT,
                .geomType           = RG_GEOMETRY_TYPE_STATIC_MOVABLE,
                .passThroughType    = RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE,
                .vertexCount        = std::size( s_CubePositions ),
                .pVertices          = GetCubeVertices(),
                .layerColors        = { { 0.5f, 0.5f, 1.0f, 1.0f } },
                .defaultRoughness   = 1.0f,
                .defaultMetallicity = 0.0f,
                .geomMaterial       = { RG_NO_MATERIAL },
                .transform          = { {
                    { 1, 0, 0, 0 },
                    { 0, 1, 0, 0 },
                    { 0, 0, 1, 0 },
                } },
            };
            r = rgUploadGeometry( instance, &movable );
            RG_CHECK( r );
        }
        r = rgSubmitStaticGeometries( instance );
        RG_CHECK( r );
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


        // transform of movable geometry can be changed
        {
            RgUpdateTransformInfo update = {
                .movableStaticUniqueID = MovableGeomUniqueID,
                .transform             = { {
                    { 1, 0, 0, ctl_MoveBoxes ? 5.0f - 0.05f * ( frameId % 200 ) : -1.0f },
                    { 0, 1, 0, 0 },
                    { 0, 0, 1, -7 },
                } },
            };
            r = rgUpdateGeometryTransform( instance, &update );
            RG_CHECK( r );
        }


        // dynamic geometry must be uploaded each frame
        {
            RgGeometryUploadInfo dynamicGeomInfo = {
                .uniqueID           = DynamicGeomUniqueID,
                .flags              = RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT,
                .geomType           = RG_GEOMETRY_TYPE_DYNAMIC,
                .vertexCount        = std::size( s_CubePositions ),
                .pVertices          = GetCubeVertices(),
                .layerColors        = { { 1.0f, 0.88f, 0.6f, 1.0f } },
                .defaultRoughness   = ctl_Roughness,
                .defaultMetallicity = ctl_Metallicity,
                .geomMaterial       = { RG_NO_MATERIAL },
                .transform          = { {
                    { 1, 0, 0, ctl_MoveBoxes ? 5.0f - 0.05f * ( ( frameId + 30 ) % 200 ) : 1.0f },
                    { 0, 1, 0, 0 },
                    { 0, 0, 1, -7 },
                } },
            };
            r = rgUploadGeometry( instance, &dynamicGeomInfo );
            RG_CHECK( r );
        }


        // upload world-space rasterized geometry for non-expensive transparency
        {
            RgRasterizedGeometryUploadInfo raster = {
                .renderType    = RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT,
                .vertexCount   = std::size( s_QuadPositions ),
                .pVertices     = GetQuadVertices(),
                .transform     = { {
                    { 1, 0, 0, -0.5f },
                    { 0, 1, 0, 0.5f },
                    { 0, 0, 1, -8 },
                } },
                .color         = { 1.0f, 1.0f, 1.0f, 1.0f },
                .material      = RG_NO_MATERIAL,
                .pipelineState = RG_RASTERIZED_GEOMETRY_STATE_DEPTH_TEST |
                                 RG_RASTERIZED_GEOMETRY_STATE_DEPTH_WRITE,
            };
            r = rgUploadRasterizedGeometry( instance, &raster, nullptr, nullptr );
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
                .direction              = { -1, -10, -1 },
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
                .skyCubemap         = skybox,
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
                .volumetricFar = 1000.0f,
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

        .pfnPrint = []( const char* pMessage, void* pUserData ) {
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