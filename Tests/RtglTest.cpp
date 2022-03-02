#include <chrono>
#include <iostream>


#define RG_USE_SURFACE_WIN32
#include <RTGL1/RTGL1.h>

#define RG_CHECK(x) assert((x) == RG_SUCCESS)


#pragma region BOILERPLATE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>


#define ASSET_DIRECTORY "../../"


static GLFWwindow   *g_GlfwHandle;

static glm::vec3    ctl_CameraPosition      = glm::vec3(0, -1.5f, 4.5f);
static glm::vec3    ctl_CameraDirection     = glm::vec3(0, 0, -1);
static glm::vec3    ctl_LightPosition       = glm::vec3(0, 0, 1);
static float        ctl_LightIntensity      = 1.0f;
static float        ctl_LightCount          = 0.0f;
static float        ctl_SunIntensity        = 1.0f;
static float        ctl_SkyIntensity        = 1.0f;
static RgBool32     ctl_SkyboxEnable        = 1;
static float        ctl_Roughness           = 0.5f;
static float        ctl_Metallicity         = 0.5f;
static RgBool32     ctl_MoveBoxes           = 0;
static RgBool32     ctl_ShowGradients       = 0;
static RgBool32     ctl_ReloadShaders       = 0;

static bool ProcessWindow()
{
    if (glfwWindowShouldClose(g_GlfwHandle)) return false;
    glfwPollEvents(); return true;
}

static void ProcessInput()
{
#define IsPressed(key) (glfwGetKey(g_GlfwHandle, (key)) == GLFW_PRESS)
#define Clamp(x, xmin, xmax) std::max(std::min((x), xmax), xmin)
    auto ControlFloat = [] (int key, float &value, float speed, float minval = 0.0f, float maxval = 1.0f) {
        if (IsPressed(key)) {
            if (IsPressed(GLFW_KEY_KP_ADD))      value += speed;
            if (IsPressed(GLFW_KEY_KP_SUBTRACT)) value -= speed;
        }
        value = Clamp(value, minval, maxval);
    };
    static auto s_lastTimePressed = std::chrono::system_clock::now();
    auto ControlSwitch = [] (int key, uint32_t &value, uint32_t stateCount = 2) {
        if (IsPressed(key)) {    
            float secondsSinceLastTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - s_lastTimePressed).count() / 1000.0f;
            if (secondsSinceLastTime < 0.5f) return;
            value = (value + 1) % stateCount;
            s_lastTimePressed = std::chrono::system_clock::now();
        }
    };

    float cameraSpeed = 5.0f;
    float delta = 1.0 / 60.0f;
    const glm::vec3 d = ctl_CameraDirection;
    const glm::vec3 u = glm::vec3(0, 1, 0);
    const glm::vec3 r = glm::cross(d, u);

    if (IsPressed(GLFW_KEY_W))      ctl_CameraPosition  += d * delta * cameraSpeed;
    if (IsPressed(GLFW_KEY_S))      ctl_CameraPosition  -= d * delta * cameraSpeed;
    if (IsPressed(GLFW_KEY_D))      ctl_CameraPosition  += r * delta * cameraSpeed;
    if (IsPressed(GLFW_KEY_A))      ctl_CameraPosition  -= r * delta * cameraSpeed;
    if (IsPressed(GLFW_KEY_E))      ctl_CameraPosition  += u * delta * cameraSpeed;
    if (IsPressed(GLFW_KEY_Q))      ctl_CameraPosition  -= u * delta * cameraSpeed;

    if (IsPressed(GLFW_KEY_LEFT))   ctl_CameraDirection  = glm::rotate(ctl_CameraDirection,  delta * 2, glm::vec3(0, 1, 0));
    if (IsPressed(GLFW_KEY_RIGHT))  ctl_CameraDirection  = glm::rotate(ctl_CameraDirection, -delta * 2, glm::vec3(0, 1, 0));

    if (IsPressed(GLFW_KEY_KP_8))   ctl_LightPosition[2] += delta * 5;
    if (IsPressed(GLFW_KEY_KP_5))   ctl_LightPosition[2] -= delta * 5;
    if (IsPressed(GLFW_KEY_KP_6))   ctl_LightPosition[0] += delta * 5;
    if (IsPressed(GLFW_KEY_KP_4))   ctl_LightPosition[0] -= delta * 5;
    if (IsPressed(GLFW_KEY_KP_9))   ctl_LightPosition[1] += delta * 5;
    if (IsPressed(GLFW_KEY_KP_7))   ctl_LightPosition[1] -= delta * 5;

    ControlFloat(GLFW_KEY_R,        ctl_Roughness,          delta,      0, 1);
    ControlFloat(GLFW_KEY_M,        ctl_Metallicity,        delta,      0, 1);
    ControlFloat(GLFW_KEY_Y,        ctl_LightIntensity,     delta,      0, 1000);
    ControlFloat(GLFW_KEY_Y,        ctl_LightCount,         delta * 5,  0, 1000);
    ControlFloat(GLFW_KEY_I,        ctl_SunIntensity,       delta,      0, 1000);
    ControlFloat(GLFW_KEY_O,        ctl_SkyIntensity,       delta,      0, 1000);

    ctl_ReloadShaders = false;
    ControlSwitch(GLFW_KEY_TAB,     ctl_SkyboxEnable);
    ControlSwitch(GLFW_KEY_Z,       ctl_MoveBoxes);
    ControlSwitch(GLFW_KEY_G,       ctl_ShowGradients);
    ControlSwitch(GLFW_KEY_H,       ctl_ReloadShaders);
}

static double GetCurrentTimeInSeconds()
{
    static auto s_TimeStart = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - s_TimeStart).count() / 1000.0;
}

#define CUBEMAP_DIRECTORY ASSET_DIRECTORY"Cubemap/"

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

static const RgFloat3D s_QuadPositions[] = {
    {0,0,0}, {0,1,0}, {1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
};
static const RgFloat2D s_QuadTexCoords[] = {
    {0,0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {1, 1},
};
static const uint32_t s_QuadColorsABGR[] = {
    0xF0FF0000, 0xF0FFFFFF, 0xF0FFFFFF, 0xF0FFFFFF, 0xFFFFFFFF, 0xFF00FF00,
};
#pragma endregion BOILERPLATE





static void MainLoop(RgInstance instance)
{
    RgResult    r           = RG_SUCCESS;
    uint64_t    frameId     = 0;
    RgMaterial  material    = RG_NO_MATERIAL;
    RgCubemap   skybox      = RG_NO_MATERIAL;

    const uint64_t MovableGeomUniqueID = 200;


    // some resources can be initialized out of frame
    {
        RgStaticMaterialCreateInfo textureInfo =
        {
            .pRelativePath = ASSET_DIRECTORY"TestImage.ktx2"
        };
        r = rgCreateStaticMaterial(instance, &textureInfo, &material);
        RG_CHECK(r);


#if 0
        RgCubemapCreateInfo skyboxInfo = 
        {
            .relativePathFaces = {
                CUBEMAP_DIRECTORY"px.ktx2", CUBEMAP_DIRECTORY"py.ktx2", CUBEMAP_DIRECTORY"pz.ktx2", 
                CUBEMAP_DIRECTORY"nx.ktx2", CUBEMAP_DIRECTORY"ny.ktx2", CUBEMAP_DIRECTORY"nz.ktx2", 
            },
            .useMipmaps = true,
            .isSRGB = true
        };
        r = rgCreateCubemap(instance, &skyboxInfo, &skybox);
        RG_CHECK(r);
#endif


        // upload static scene
        r = rgStartNewScene(instance);
        RG_CHECK(r);
        {
            const RgGeometryUploadInfo staticCubeGeomTemplate = 
            {
                .flags                  = RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT,
                .vertexCount            = std::size(s_CubePositions),
                .pVertexData            = s_CubePositions,
                .pNormalData            = nullptr,
                .pTexCoordLayerData     = { s_CubeTexCoords },
                .indexCount             = 0,
                .pIndexData             = nullptr,
                .layerColors            = { { 1.0f, 1.0f, 1.0f, 1.0f } },
                .defaultRoughness       = 1.0f,
                .defaultMetallicity     = 0.0f,
                .geomMaterial           = { RG_NO_MATERIAL },
                .transform = {
                    1, 0, 0, 0,
                    0, 1, 0, 0,
                    0, 0, 1, 0
                }
            };

            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                info.geomType = RG_GEOMETRY_TYPE_STATIC;

                // bottom
                info.uniqueID = 100;
                info.transform = {
                    5, 0, 0, 0,
                    0, 1, 0, -4,
                    0, 0, 5 , 0
                };
                info.layerColors[0] = { 1, 1, 1, 1 };

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }

            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                info.geomType = RG_GEOMETRY_TYPE_STATIC;

                // top
                info.uniqueID = 101;
                info.transform = {
                    5, 0, 0, 0,
                    0, 1, 0, +4,
                    0, 0, 5, 0
                };
                info.layerColors[0] = { 1, 1, 1, 1 };

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }
                       
            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                info.geomType = RG_GEOMETRY_TYPE_STATIC;

                // left, red
                info.uniqueID = 102;
                info.transform = {
                    1, 0, 0, -3,
                    0, 9, 0, 0,
                    0, 0, 5, 0
                };
                info.layerColors[0] = { 1, 0, 0, 1 };

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }
      
            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                info.geomType = RG_GEOMETRY_TYPE_STATIC;

                // right, green
                info.uniqueID = 103;
                info.transform = {
                    1, 0, 0, +3,
                    0, 9, 0, 0,
                    0, 0, 5, 0
                };
                info.layerColors[0] = { 0, 1, 0, 1 };

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }
      
            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                info.geomType = RG_GEOMETRY_TYPE_STATIC;

                // back
                info.uniqueID = 104;
                info.transform = {
                    7, 0, 0, 0,
                    0, 9, 0, 0,
                    0, 0, 1, -3
                };
                info.layerColors[0] = { 1, 1, 1, 1 };

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }
      
            {
                RgGeometryUploadInfo info = staticCubeGeomTemplate;
                // movable static
                info.geomType = RG_GEOMETRY_TYPE_STATIC_MOVABLE;
                info.uniqueID = MovableGeomUniqueID;

                // left, tall box - transform is updated in a loop
                info.layerColors[0] = { 1, 1, 1, 1 };
                info.geomMaterial.layerMaterials[0] = material;

                r = rgUploadGeometry(instance, &info);
                RG_CHECK(r);
            }
        }
        r = rgSubmitStaticGeometries(instance);
        RG_CHECK(r);
    }


    while (ProcessWindow())
    {
        ProcessInput();

        {
            int width, height; glfwGetWindowSize(g_GlfwHandle, &width, &height);

            RgStartFrameInfo startInfo = {
                .surfaceSize = { (uint32_t)width, (uint32_t)height },
                .requestVSync = true,
                .requestShaderReload = ctl_ReloadShaders
            };
            r = rgStartFrame(instance, &startInfo);
            RG_CHECK(r);
        }


        // transform of movable geometry can be changed
        RgUpdateTransformInfo transformUpdateInfo = 
        {
            .movableStaticUniqueID = MovableGeomUniqueID,
            .transform = {
                1, 0, 0, ctl_MoveBoxes ? 5.0f - 0.05f * (frameId % 200) : -1.0f,
                0, 2, 0, -3.5 + 1,
                0, 0, 1, -0.5
            }
        };
        r = rgUpdateGeometryTransform(instance, &transformUpdateInfo);
        RG_CHECK(r);


        // dynamic geometry must be uploaded each frame
        RgGeometryUploadInfo dynamicGeomInfo = 
        {
            .uniqueID               = 300,
            .flags                  = RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT,
            .geomType               = RG_GEOMETRY_TYPE_DYNAMIC,
            .vertexCount            = std::size(s_CubePositions),
            .pVertexData            = s_CubePositions,
            .pNormalData            = nullptr,
            .pTexCoordLayerData     = { s_CubeTexCoords },
            .indexCount             = 0,
            .pIndexData             = nullptr,
            .layerColors            = { { 1.0f, 1.0f, 1.0f, 1.0f } },
            .defaultRoughness       = ctl_Roughness,
            .defaultMetallicity     = ctl_Metallicity,
            .geomMaterial           = { RG_NO_MATERIAL },
            .transform = {
                1, 0, 0, ctl_MoveBoxes ? 5.0f - 0.05f * ((frameId + 30) % 200) : 1.0f,
                0, 1, 0, -3.5 + 0.5,
                0, 0, 1, 0
            }
        };
        r = rgUploadGeometry(instance, &dynamicGeomInfo);
        RG_CHECK(r);


        // upload rasterized geometry
        RgRasterizedGeometryVertexArrays rasterVertData = 
        {
            .pVertexData    = s_QuadPositions,
            .pTexCoordData  = s_QuadTexCoords,
            .pColorData     = s_QuadColorsABGR,
            .vertexStride   = 3 * sizeof(float),
            .texCoordStride = 2 * sizeof(float),
            .colorStride    = sizeof(uint32_t),
        };
        RgRasterizedGeometryUploadInfo raster = 
        {
            // world-space
            .renderType     = RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT,
            .vertexCount    = 6,
            .pArrays        = &rasterVertData,
            .transform = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0
            },
            .color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .material = RG_NO_MATERIAL,
            .blendEnable = RG_FALSE,
            .depthTest = true,
            .depthWrite = true
        };
        r = rgUploadRasterizedGeometry(instance, &raster, nullptr, nullptr);
        RG_CHECK(r);


        RgDecalUploadInfo decalInfo =
        {    
            .transform = {
                2, 0, 0, 0,
                0, 2, 0, 0,
                0, 0, 2, 0
            },
            .material = RG_NO_MATERIAL
        };
        r = rgUploadDecal(instance, &decalInfo);
        RG_CHECK(r);


        // upload sun
        RgDirectionalLightUploadInfo dirLight = 
        {
            .color                  = { ctl_SunIntensity, ctl_SunIntensity, ctl_SunIntensity },
            .direction              = { -2, -1, -1 },
            .angularDiameterDegrees = 0.5f
        };
        r = rgUploadDirectionalLight(instance, &dirLight);
        RG_CHECK(r);


#if 0
        // upload sphere lights
        {
            uint32_t count = (frameId % 2) * 64 + 128;

            for (uint64_t i = 0; i < count; i++)
            {
                RgSphericalLightUploadInfo sphLight = 
                {
                    .uniqueID           = i,
                    .color              = { ctl_LightIntensity, ctl_LightIntensity, ctl_LightIntensity },
                    .position           = { ctl_LightPosition[0] + i * 3, ctl_LightPosition[1], ctl_LightPosition[2] },
                    .radius             = 0.25f,
                    .falloffDistance    = 10.0f,
                };
                r = rgUploadSphericalLight(instance, &l);        
                RG_CHECK(r);    
            }
        }
#endif


        glm::mat4 persp = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 10000.0f); persp[1][1] *= -1;
        glm::mat4 view = glm::lookAt(ctl_CameraPosition, ctl_CameraPosition + ctl_CameraDirection, { 0,1,0 });


        RgDrawFrameSkyParams skyParams = 
        {
            .skyType            = ctl_SkyboxEnable ? RG_SKY_TYPE_CUBEMAP : RG_SKY_TYPE_COLOR,
            .skyColorDefault    = { 0.71f, 0.88f, 1.0f },
            .skyColorMultiplier = ctl_SkyIntensity,
            .skyViewerPosition  = { 0, 0, 0 },
            .skyCubemap         = skybox
        };

        RgDrawFrameDebugParams debugParams = 
        {
            .showGradients = ctl_ShowGradients
        };

        RgDrawFrameRenderResolutionParams resolutionParams = 
        {
            .upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR,
            .resolutionMode = RG_RENDER_RESOLUTION_MODE_BALANCED,
        };

        RgDrawFrameInfo frameInfo = 
        {
            .fovYRadians = glm::radians(75.0f),
            .rayCullMaskWorld = RG_DRAW_FRAME_RAY_CULL_WORLD_0_BIT,
            .rayLength = 10000.0f,
            .currentTime = GetCurrentTimeInSeconds(),
            .pRenderResolutionParams = &resolutionParams,
            .pSkyParams = &skyParams,
            .pDebugParams = &debugParams,
        };
        // GLM is column major, copy matrix data directly
        memcpy(frameInfo.projection, &persp[0][0], 16 * sizeof(float));
        memcpy(frameInfo.view, &view[0][0], 16 * sizeof(float));

        r = rgDrawFrame(instance, &frameInfo);
        RG_CHECK(r);


        frameId++;
    }
}


int main()
{
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    g_GlfwHandle = glfwCreateWindow(1600, 900, "RTGL1 Test", nullptr, nullptr);


    RgResult r; 
    RgInstance instance;

    RgWin32SurfaceCreateInfo win32Info =
    {
        .hinstance = GetModuleHandle(NULL),
        .hwnd = glfwGetWin32Window(g_GlfwHandle),
    };

    RgInstanceCreateInfo info = 
    {
        .pAppName                           = "RTGL1 Test",
        .pAppGUID                           = "459d6734-62a6-4d47-927a-bedcdb0445c5",

        .pWin32SurfaceInfo                  = &win32Info,

        .enableValidationLayer              = true,
        .pfnPrint                           = [] (const char *pMessage, void *pUserData)
                                            {
                                                std::cout << pMessage << std::endl;
                                            },

        .pShaderFolderPath                  = ASSET_DIRECTORY,
        .pBlueNoiseFilePath                 = ASSET_DIRECTORY"BlueNoise_LDR_RGBA_128.ktx2",

        .primaryRaysMaxAlbedoLayers         = 1,
        .indirectIlluminationMaxAlbedoLayers= 1,

        .rayCullBackFacingTriangles         = false,

        .rasterizedMaxVertexCount           = 4096,
        .rasterizedMaxIndexCount            = 2048,
        
        .rasterizedSkyMaxVertexCount        = 4096,
        .rasterizedSkyMaxIndexCount         = 2048,
        .rasterizedSkyCubemapSize           = 256,

        .maxTextureCount                    = 1024,
        .overridenAlbedoAlphaTextureIsSRGB  = true,
        .pWaterNormalTexturePath            = ASSET_DIRECTORY"WaterNormal_n.ktx2",

        .vertexPositionStride               = 3 * sizeof(float),
        .vertexNormalStride                 = 3 * sizeof(float),
        .vertexTexCoordStride               = 2 * sizeof(float),
        .vertexColorStride                  = sizeof(uint32_t),
    };

    r = rgCreateInstance(&info, &instance);
    RG_CHECK(r);

    MainLoop(instance);

    r = rgDestroyInstance(instance);
    RG_CHECK(r);

    
    glfwDestroyWindow(g_GlfwHandle);
    glfwTerminate();
}