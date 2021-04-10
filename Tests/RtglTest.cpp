#include <iostream>
#include <vector>

#include <RTGL1/RTGL1.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "Libs/tinyobjloader/tiny_obj_loader.h"

#include <fstream>
#include <chrono>

#define RG_CHECKERROR(x) assert(x == RG_SUCCESS)

struct Window
{
    Window() : width(1600), height(900), extensionCount(0)
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        glfwHandle = glfwCreateWindow(width, height, "RTGL1 Test", nullptr, nullptr);
        UpdateSize();

        extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    }

    ~Window()
    {
        glfwDestroyWindow(glfwHandle);
        glfwTerminate();
    }

    void CreateVkSurface(uint64_t vkInstance, uint64_t *pResultVkSurfaceKHR)
    {
        VkInstance instance = reinterpret_cast<VkInstance>(vkInstance);

        VkSurfaceKHR surface;
        glfwCreateWindowSurface(instance, glfwHandle, nullptr, &surface);

        *pResultVkSurfaceKHR = reinterpret_cast<uint64_t>(surface);
    }

    void UpdateSize()
    {
        glfwGetFramebufferSize(glfwHandle, static_cast<int *>(&width), static_cast<int *>(&height));
    }

    GLFWwindow      *glfwHandle;
    int             width, height;
    const char      **extensions;
    uint32_t        extensionCount;
};

static std::ofstream logFile;

static void DebugPrint(const char *msg)
{
    std::cout << msg;
    logFile << msg;
}


static std::vector<const char *> cubemapNames = {
    //"Cubemap/S (1)",
    "Cubemap/S (2)",
    //"Cubemap/S (3)",
    //"Cubemap/S (4)",
    //"Cubemap/S (5)",
    //"Cubemap/S (6)",
    "Cubemap/S (7)",
    //"Cubemap/S (8)",
    //"Cubemap/S (9)",
    //"Cubemap/S (10)",
    //"Cubemap/S (11)",
    "Cubemap/S (12)",
};


static glm::vec3 CAMERA_POS     = glm::vec3(0, 2, -8);
static glm::vec3 CAMERA_DIR     = glm::vec3(0, 0, 1);
static glm::vec3 CAMERA_UP      = glm::vec3(0, 1, 0);
static glm::vec3 LIGHT_DIR      = glm::vec3(-1, -1, -1);
static glm::vec3 LIGHT_POS      = glm::vec3(0, 4, -2);
static glm::vec3 LIGHT_COLOR    = glm::vec3(1, 1, 1);
static float LIGHT_RADIUS       = 0.2f;
static float LIGHT_SPH_COUNT    = 0.0f;
static float ROUGHNESS          = 0.5f;
static float METALLICITY        = 0.5f;
static float SUN_INTENSITY      = 1.0f;
static float LIGHT_FALLOFF      = 12.0f;
static float SKY_INTENSITY      = 1.0f;
static uint32_t SKYBOX_CURRENT  = 0;
static bool TO_MOVE             = false;
static bool SHOW_GRAD           = false;
static bool RELOAD_SHADERS      = false;

static void ProcessInput(GLFWwindow *window)
{
    float delta = 1.0 / 60.0f;

    float cameraSpeed = 5 * delta;
    float cameraRotationSpeed = 2 * delta;

    glm::vec3 r = glm::cross(CAMERA_DIR, glm::vec3(0, 1, 0));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)       CAMERA_POS  += cameraSpeed * CAMERA_DIR;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)       CAMERA_POS  -= cameraSpeed * CAMERA_DIR;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)       CAMERA_POS  -= r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)       CAMERA_POS  += r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)       CAMERA_POS  -= glm::vec3(0, 1, 0) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)       CAMERA_POS  += glm::vec3(0, 1, 0) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS)   CAMERA_DIR  = glm::rotate(CAMERA_DIR, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)   CAMERA_DIR  = glm::rotate(CAMERA_DIR, -cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS)   CAMERA_DIR  = glm::rotate(CAMERA_DIR, cameraRotationSpeed, r);
    if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS)   CAMERA_DIR  = glm::rotate(CAMERA_DIR, -cameraRotationSpeed, r);

    if (glfwGetKey(window, GLFW_KEY_KP_4)  == GLFW_PRESS)   LIGHT_POS[0] -= delta * 5;
    if (glfwGetKey(window, GLFW_KEY_KP_6)  == GLFW_PRESS)   LIGHT_POS[0] += delta * 5;
    if (glfwGetKey(window, GLFW_KEY_KP_7)  == GLFW_PRESS)   LIGHT_POS[1] -= delta * 5;
    if (glfwGetKey(window, GLFW_KEY_KP_9)  == GLFW_PRESS)   LIGHT_POS[1] += delta * 5;
    if (glfwGetKey(window, GLFW_KEY_KP_5)  == GLFW_PRESS)   LIGHT_POS[2] -= delta * 5;
    if (glfwGetKey(window, GLFW_KEY_KP_8)  == GLFW_PRESS)   LIGHT_POS[2] += delta * 5;
    
    CAMERA_UP = glm::cross(-r, CAMERA_DIR);

    if (glfwGetKey(window, GLFW_KEY_1)     == GLFW_PRESS)   LIGHT_DIR   = glm::rotate(LIGHT_DIR, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_2)     == GLFW_PRESS)   LIGHT_DIR   = glm::rotate(LIGHT_DIR, cameraRotationSpeed, glm::vec3(1, 0, 0));

    auto controlFloat = [window] (int mainKey, float &value, float speed)
    {
        if (glfwGetKey(window, mainKey) == GLFW_PRESS)
        {
            if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)      value += speed;
            if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) value -= speed;
        }
    };

    controlFloat(GLFW_KEY_R, ROUGHNESS, delta);
    controlFloat(GLFW_KEY_M, METALLICITY, delta);
    controlFloat(GLFW_KEY_I, SUN_INTENSITY, delta);
    controlFloat(GLFW_KEY_F, LIGHT_FALLOFF, delta * 10);
    controlFloat(GLFW_KEY_O, SKY_INTENSITY, delta);
    controlFloat(GLFW_KEY_T, LIGHT_RADIUS, delta);
    controlFloat(GLFW_KEY_Y, LIGHT_SPH_COUNT, delta * 5);

    ROUGHNESS       = std::max(std::min(ROUGHNESS, 1.0f), 0.0f);
    METALLICITY     = std::max(std::min(METALLICITY, 1.0f), 0.0f);
    SUN_INTENSITY   = std::max(SUN_INTENSITY, 0.0f);
    LIGHT_RADIUS    = std::max(LIGHT_RADIUS, 0.0f);
    LIGHT_SPH_COUNT = std::max(LIGHT_SPH_COUNT, 0.0f);
    LIGHT_FALLOFF   = std::max(LIGHT_FALLOFF, 0.0f);
    SKY_INTENSITY   = std::max(SKY_INTENSITY, 0.0f);
    LIGHT_COLOR     = SUN_INTENSITY * glm::vec3(1, 1, 1);
    LIGHT_DIR       = glm::normalize(LIGHT_DIR);
    RELOAD_SHADERS  = false;


    // switches
    static auto lastTimePressed = std::chrono::system_clock::now();

    auto now = std::chrono::system_clock::now();
    float secondsSinceLastTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimePressed).count() / 1000.0f;

    if (secondsSinceLastTime < 0.5f)
    {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
    {
        SKYBOX_CURRENT = ((uint64_t)SKYBOX_CURRENT + 1) % cubemapNames.size();
        lastTimePressed = now;
    }

    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
    {
        TO_MOVE = !TO_MOVE;
        lastTimePressed = now;
    }

    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
    {
        SHOW_GRAD = !SHOW_GRAD;
        lastTimePressed = now;
    }

    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
    {
        RELOAD_SHADERS = true;
        lastTimePressed = now;
    }
}

static void LoadObj(const char *path,
             std::vector<float> &_positions,
             std::vector<float> &_normals,
             std::vector<float> &_texCoords,
             std::vector<uint32_t> &_indices)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);
    assert(ret);

    uint32_t vertId = 0;

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            int fv = shapes[s].mesh.num_face_vertices[f];
            assert(fv == 3);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
                tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];

                _positions.push_back(vx);
                _positions.push_back(vy);
                _positions.push_back(vz);

                _normals.push_back(nx);
                _normals.push_back(ny);
                _normals.push_back(nz);

                _texCoords.push_back(tx);
                _texCoords.push_back(ty);

                _indices.push_back(vertId);
                vertId++;
            }

            index_offset += fv;
        }
    }

}

static float quadPositions[] =
{
    0, 0, 0,
    0, 1, 0,
    1, 0, 0,
    1, 0, 0,
    0, 1, 0,
    1, 1, 0
};

static float quadTexCoords[] =
{
    0, 0,
    0, 1,
    1, 0,
    1, 0,
    0, 1,
    1, 1,
};

static uint32_t quadColorsABGR[] =
{
    0xF0FF0000,
    0xF0FFFFFF,
    0xF0FFFFFF,
    0xF0FFFFFF,
    0xFFFFFFFF,
    0xFF00FF00,
};

static float identityMatrix43[] =
{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0
};
static float identityMatrix44[] =
{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

static void MainLoop(RgInstance instance, Window *pWindow)
{
    auto timeStart = std::chrono::system_clock::now();

    std::vector<float>       cubePositions;
    std::vector<float>       cubeNormals;
    std::vector<float>       cubeTexCoords;
    std::vector<float>       cubeTexCoordsModif;
    std::vector<uint32_t>    cubeIndices;

    // cube with extents: (-1,-1,-1) (1,1,1)
    LoadObj("../../../Cube.obj", cubePositions, cubeNormals, cubeTexCoords, cubeIndices);

    for (auto f : cubeTexCoords)
    {
        cubeTexCoordsModif.push_back(f * 0.25f);
    }

    // geometry infos
    RgGeometryUploadInfo cubeInfo = {};
    cubeInfo.vertexCount = cubePositions.size() / 3;
    cubeInfo.vertexData = cubePositions.data();
    cubeInfo.normalData = cubeNormals.data();
    cubeInfo.texCoordLayerData[0] = cubeTexCoords.data();
    cubeInfo.indexCount = cubeIndices.size();
    cubeInfo.indexData = cubeIndices.data();
    cubeInfo.layerColors[0] = { 1.0f, 1.0f, 1.0f, 1.0f };
    cubeInfo.defaultRoughness = 1;
    cubeInfo.defaultMetallicity = 0;
    cubeInfo.geomMaterial = {
        RG_NO_MATERIAL,
        RG_NO_MATERIAL,
        RG_NO_MATERIAL
    };
    cubeInfo.transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0
    };

    RgGeometryUploadInfo stInfo = cubeInfo;
    stInfo.geomType = RG_GEOMETRY_TYPE_STATIC;
    stInfo.transform = {
        30, 0, 0, 0,
        0, 1, 0, -1,
        0, 0, 30, 0
    };

    RgGeometryUploadInfo mvInfo = cubeInfo;
    mvInfo.geomType = RG_GEOMETRY_TYPE_STATIC_MOVABLE;

    RgGeometryUploadInfo dnInfo = cubeInfo;
    dnInfo.geomType = RG_GEOMETRY_TYPE_DYNAMIC;
    dnInfo.layerColors[0] = { 1.0f, 0.0f, 0.0f, 0.0f };


    // texture info
    RgStaticMaterialCreateInfo textureInfo = {};
    textureInfo.relativePath = "../../../TestImage.png";
    textureInfo.useMipmaps = RG_TRUE;


    // rasterized geometry for HUD
    RgRasterizedGeometryVertexArrays hudVertData = {};
    hudVertData.vertexData = quadPositions;
    hudVertData.texCoordData = quadTexCoords;
    hudVertData.colorData = quadColorsABGR;
    hudVertData.vertexStride = 3 * sizeof(float);
    hudVertData.texCoordStride = 2 * sizeof(float);
    hudVertData.colorStride = sizeof(uint32_t);

    RgRasterizedGeometryUploadInfo raster = {};
    raster.vertexCount = 6;
    raster.arrays = &hudVertData;
    raster.material = RG_NO_MATERIAL;
    raster.transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0
    };
    raster.color = { 1, 1, 1, 1 };
    raster.blendEnable = RG_TRUE;


    RgResult    r           = RG_SUCCESS;
    uint64_t    frameCount  = 0;
    RgMaterial  material    = RG_NO_MATERIAL;

    std::vector<RgCubemap> skyboxes(cubemapNames.size());


    while (!glfwWindowShouldClose(pWindow->glfwHandle))
    {
        glfwPollEvents();
        pWindow->UpdateSize();
        ProcessInput(pWindow->glfwHandle);

        {
            RgStartFrameInfo startInfo = {};
            startInfo.surfaceWidth = (uint32_t)pWindow->width;
            startInfo.surfaceHeight = (uint32_t)pWindow->height;
            startInfo.requestVSync = true;
            startInfo.requestShaderReload = RELOAD_SHADERS;

            r = rgStartFrame(instance, &startInfo);
            RG_CHECKERROR(r);
        }

        if (frameCount == 0)
        {
            // upload material
            r = rgCreateStaticMaterial(instance, &textureInfo, &material);
            RG_CHECKERROR(r);


            // create skyboxes
            for (uint32_t i = 0; i < cubemapNames.size(); i++)
            {
                std::string skyboxFolderPath = std::string("../../../") + cubemapNames[i] + "/";

                std::string px = skyboxFolderPath + "px.png";
                std::string py = skyboxFolderPath + "py.png";
                std::string pz = skyboxFolderPath + "pz.png";
                std::string nx = skyboxFolderPath + "nx.png";
                std::string ny = skyboxFolderPath + "ny.png";
                std::string nz = skyboxFolderPath + "nz.png";

                RgCubemapCreateInfo skyboxInfo = {};
                skyboxInfo.useMipmaps = RG_TRUE;
                skyboxInfo.isSRGB = RG_TRUE;
                skyboxInfo.relativePathFaces.positiveX = px.c_str();
                skyboxInfo.relativePathFaces.positiveY = py.c_str();
                skyboxInfo.relativePathFaces.positiveZ = pz.c_str();
                skyboxInfo.relativePathFaces.negativeX = nx.c_str();
                skyboxInfo.relativePathFaces.negativeY = ny.c_str();
                skyboxInfo.relativePathFaces.negativeZ = nz.c_str();

                r = rgCreateCubemap(instance, &skyboxInfo, &skyboxes[i]);
                RG_CHECKERROR(r);    
            }


            // start static scene upload
            r = rgStartNewScene(instance);
            RG_CHECKERROR(r);

            stInfo.uniqueID = 0;
            r = rgUploadGeometry(instance, &stInfo);
            RG_CHECKERROR(r);

            mvInfo.uniqueID = 1;
            mvInfo.geomMaterial.layerMaterials[0] = material;
            r = rgUploadGeometry(instance, &mvInfo);
            RG_CHECKERROR(r);

            // upload static geometry
            r = rgSubmitStaticGeometries(instance);
            RG_CHECKERROR(r);
        }


        // update transform of movable geometry
        RgUpdateTransformInfo updateInfo = {};
        updateInfo.movableStaticUniqueID = 1;
        updateInfo.transform = {
            0.3f, 0, 0, TO_MOVE ? 5.0f - 0.05f * (frameCount % 200) : -2.5f,
            0, 4, 0, 4,
            0, 0, 0.3f, 0
        };
        r = rgUpdateGeometryTransform(instance, &updateInfo);
        RG_CHECKERROR(r);


        RgUpdateTexCoordsInfo texCoordsInfo = {};
        texCoordsInfo.staticUniqueID = 1;
        texCoordsInfo.texCoordLayerData[0] = (frameCount % 120) > 60 ? cubeTexCoords.data() : cubeTexCoordsModif.data();
        texCoordsInfo.vertexOffset = 0;
        texCoordsInfo.vertexCount = cubeInfo.vertexCount;
        r = rgUpdateGeometryTexCoords(instance, &texCoordsInfo);
        RG_CHECKERROR(r);


        // dynamic geometry must be uploaded each frame
        dnInfo.defaultMetallicity = METALLICITY;
        dnInfo.defaultRoughness = ROUGHNESS;

        dnInfo.transform = {
            0.3f, 0, 0, TO_MOVE ? 5.0f - 0.05f * ((frameCount + 30) % 200) : 0,
            0, 4, 0, 0,
            0, 0, 0.3f, 0
        };
        dnInfo.uniqueID = 2;
        rgUploadGeometry(instance, &dnInfo);

        dnInfo.transform = {
            0.3f, 0, 0, TO_MOVE ? 5.0f - 0.05f * ((frameCount + 60) % 200) : 2.5f,
            0, 4, 0, 0,
            0, 0, 0.3f, 0
        };
        dnInfo.uniqueID = 3;
        dnInfo.passThroughType = RG_GEOMETRY_PASS_THROUGH_TYPE_REFLECT;
        rgUploadGeometry(instance, &dnInfo);

        // upload rasterized geometry
        r = rgUploadRasterizedGeometry(instance, &raster, nullptr, nullptr);
        RG_CHECKERROR(r);


        // upload light
        RgDirectionalLightUploadInfo dirLight = {};
        dirLight.color = { LIGHT_COLOR[0], LIGHT_COLOR[1], LIGHT_COLOR[2] };
        dirLight.direction = { LIGHT_DIR[0], LIGHT_DIR[1], LIGHT_DIR[2] };
        dirLight.angularDiameterDegrees = 0.5f;
        r = rgUploadDirectionalLight(instance, &dirLight);

        RgSphericalLightUploadInfo l = {};
        l.color =  { LIGHT_COLOR[0], LIGHT_COLOR[1], LIGHT_COLOR[2] };
        l.radius = LIGHT_FALLOFF / 10.0f;
        l.falloffDistance = LIGHT_FALLOFF;

        uint32_t count = (uint32_t)LIGHT_SPH_COUNT;
        float distance = LIGHT_RADIUS;

        for (uint64_t i = 0; i < count * count; i++)
        {
            l.uniqueID = i;
            l.position = { LIGHT_POS[0] + distance * (i / count), LIGHT_POS[1], LIGHT_POS[2] + distance * (i % count) };
            r = rgUploadSphericalLight(instance, &l);            
        }

        // submit frame to be rendered
        RgDrawFrameInfo frameInfo = {};
        frameInfo.renderWidth = pWindow->width;
        frameInfo.renderHeight = pWindow->height;

        auto tm = std::chrono::system_clock::now() - timeStart;
        frameInfo.currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(tm).count() / 1000.0f;

        // GLM is column major, copy matrix data directly
        glm::mat4 persp = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 10000.0f);
        memcpy(frameInfo.projection, &persp[0][0], 16 * sizeof(float));

        glm::mat4 view = glm::lookAt(CAMERA_POS, CAMERA_POS + CAMERA_DIR, CAMERA_UP);
        memcpy(frameInfo.view, &view[0][0], 16 * sizeof(float));

        frameInfo.skyColorDefault = { 0.71f, 0.88f, 1.0f };
        frameInfo.skyType = RG_SKY_TYPE_CUBEMAP;
        frameInfo.skyCubemap = skyboxes[SKYBOX_CURRENT];
        frameInfo.skyColorMultiplier = SKY_INTENSITY;
        frameInfo.skyViewerPosition = { CAMERA_POS.x / 50, CAMERA_POS.y / 50, CAMERA_POS.z / 50 };

        frameInfo.dbgShowGradients = SHOW_GRAD;

        r = rgDrawFrame(instance, &frameInfo);
        RG_CHECKERROR(r);

        frameCount++;
    }
}

int main()
{
    RgResult r          = RG_SUCCESS;;
    RgInstance instance = RG_NULL_HANDLE;

    static Window window = Window();

    logFile.open("LogOutput.txt");

    try
    {

        RgInstanceCreateInfo info = {};

        info.name                       = "RTGL1 Test";
        info.physicalDeviceIndex        = 0;
        info.enableValidationLayer      = RG_TRUE;

        info.vertexPositionStride       = 3 * sizeof(float);
        info.vertexNormalStride         = 3 * sizeof(float);
        info.vertexTexCoordStride       = 2 * sizeof(float);
        info.vertexColorStride          = sizeof(uint32_t);
        info.rasterizedMaxVertexCount   = 4096;
        info.rasterizedMaxIndexCount    = 2048;

        info.ppWindowExtensions         = window.extensions;
        info.windowExtensionCount       = window.extensionCount;

        info.pfnCreateSurface = [](uint64_t vkInstance, uint64_t *pResultVkSurfaceKHR)
        {
            window.CreateVkSurface(vkInstance, pResultVkSurfaceKHR);
        };

        info.pfnDebugPrint = DebugPrint;

        r = rgCreateInstance(&info, &instance);
        RG_CHECKERROR(r);

        MainLoop(instance, &window);

        r = rgDestroyInstance(instance);
        RG_CHECKERROR(r);

    }
    catch(std::exception &e)
    {
        logFile << e.what() << '\n';
    }

    logFile.close();
}