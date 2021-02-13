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


static glm::vec3 CAMERA_POS     = glm::vec3(0, 2, -8);
static glm::vec3 CAMERA_DIR     = glm::vec3(0, 0, 1);
static glm::vec3 CAMERA_UP      = glm::vec3(0, 1, 0);
static glm::vec3 LIGHT_DIR      = glm::vec3(1, 1, 1);

static void ProcessInput(GLFWwindow *window)
{
    float cameraSpeed = 60.0f / 60.0f;
    float cameraRotationSpeed = 5 / 60.0f;

    glm::vec3 r = glm::cross(CAMERA_DIR, glm::vec3(0, 1, 0));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)       CAMERA_POS += cameraSpeed * CAMERA_DIR;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)       CAMERA_POS -= cameraSpeed * CAMERA_DIR;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)       CAMERA_POS -= r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)       CAMERA_POS += r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)       CAMERA_POS -= glm::vec3(0, 1, 0) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)       CAMERA_POS += glm::vec3(0, 1, 0) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)    CAMERA_DIR = glm::rotate(CAMERA_DIR, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)   CAMERA_DIR = glm::rotate(CAMERA_DIR, -cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)      CAMERA_DIR = glm::rotate(CAMERA_DIR, cameraRotationSpeed, r);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)    CAMERA_DIR = glm::rotate(CAMERA_DIR, -cameraRotationSpeed, r);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)       LIGHT_DIR = glm::rotate(LIGHT_DIR, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)       LIGHT_DIR = glm::rotate(LIGHT_DIR, cameraRotationSpeed, glm::vec3(1, 0, 0));

    CAMERA_UP = glm::cross(-r, CAMERA_DIR);
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
    0xFFFF0000,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
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
    std::vector<uint32_t>    cubeIndices;

    // cube with extents: (-1,-1,-1) (1,1,1)
    LoadObj("../../../Cube.obj", cubePositions, cubeNormals, cubeTexCoords, cubeIndices);


    // geometry infos
    RgGeometryUploadInfo cubeInfo = {};
    cubeInfo.vertexCount = cubePositions.size() / 3;
    cubeInfo.vertexData = cubePositions.data();
    cubeInfo.normalData = cubeNormals.data();
    cubeInfo.texCoordLayerData[0] = cubeTexCoords.data();
    cubeInfo.indexCount = cubeIndices.size();
    cubeInfo.indexData = cubeIndices.data();
    cubeInfo.color[0] = cubeInfo.color[1] = cubeInfo.color[2] = cubeInfo.color[3] = 1.0f;
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
    stInfo.transform =
    {
        30, 0, 0, 0,
        0, 1, 0, -1,
        0, 0, 30, 0
    };

    const void const  * a = nullptr;

    RgGeometryUploadInfo mvInfo = cubeInfo;
    mvInfo.geomType = RG_GEOMETRY_TYPE_STATIC_MOVABLE;

    RgGeometryUploadInfo dnInfo = cubeInfo;
    dnInfo.geomType = RG_GEOMETRY_TYPE_DYNAMIC;
    dnInfo.color[0] = 1.0f;
    dnInfo.color[1] = dnInfo.color[2] = dnInfo.color[3] = 0.0f;


    // texture info
    RgStaticMaterialCreateInfo textureInfo = {};
    textureInfo.relativePath = "../../../TestImage.png";
    textureInfo.useMipmaps = RG_TRUE;


    // rasterized geometry for HUD
    RgRasterizedGeometryUploadInfo raster = {};
    raster.vertexData = quadPositions;
    raster.vertexCount = 6;
    raster.vertexStride = 3 * sizeof(float);
    raster.texCoordData = quadTexCoords;
    raster.texCoordStride = 2 * sizeof(float);
    raster.colorData = quadColorsABGR;
    raster.colorStride = sizeof(uint32_t);
    raster.textures =
    {
        RG_NO_MATERIAL,
        RG_NO_MATERIAL,
        RG_NO_MATERIAL
    };
    memcpy(raster.viewProjection, identityMatrix44, 16 * sizeof(float));
    raster.viewport.x = -800;
    raster.viewport.y = -450;
    raster.viewport.width = 100;
    raster.viewport.height = 100;


    RgResult    r           = RG_SUCCESS;
    uint64_t    frameCount  = 0;
    float       toMove  = 0;
    RgMaterial  material    = RG_NO_MATERIAL;
    RgGeometry  movableGeom = UINT32_MAX;


    while (!glfwWindowShouldClose(pWindow->glfwHandle))
    {
        glfwPollEvents();
        pWindow->UpdateSize();
        ProcessInput(pWindow->glfwHandle);

        r = rgStartFrame(instance, (uint32_t)pWindow->width, (uint32_t)pWindow->height, true);
        RG_CHECKERROR(r);

        if (frameCount == 0)
        {
            // upload material
            r = rgCreateStaticMaterial(instance, &textureInfo, &material);
            RG_CHECKERROR(r);


            // start static scene upload
            r = rgStartNewScene(instance);
            RG_CHECKERROR(r);

            r = rgUploadGeometry(instance, &stInfo, nullptr);
            RG_CHECKERROR(r);

            mvInfo.geomMaterial.layerMaterials[0] = material;
            r = rgUploadGeometry(instance, &mvInfo, &movableGeom);
            RG_CHECKERROR(r);

            // upload static geometry
            r = rgSubmitStaticGeometries(instance);
            RG_CHECKERROR(r);
        }


        // update transform of movable geometry
        RgUpdateTransformInfo updateInfo = {};
        updateInfo.movableStaticGeom = movableGeom;
        updateInfo.transform = {
            0.3f, 0, 0, 5.0f - 0.05f * (frameCount % 200),
            0, 4, 0, 4,
            0, 0, 0.3f, 0
        };
        r = rgUpdateGeometryTransform(instance, &updateInfo);
        RG_CHECKERROR(r);


        // dynamic geometry must be uploaded each frame
        dnInfo.transform = {
            0.3f, 0, 0, 5.0f - 0.05f * ((frameCount + 30) % 200),
            0, 4, 0, 4,
            0, 0, 0.3f, 0
        };
        rgUploadGeometry(instance, &dnInfo, nullptr);

        dnInfo.transform = {
            0.3f, 0, 0, 5.0f - 0.05f * ((frameCount + 60) % 200),
            0, 4, 0, 4,
            0, 0, 0.3f, 0
        };
        rgUploadGeometry(instance, &dnInfo, nullptr);

        // upload rasterized geometry
        r = rgUploadRasterizedGeometry(instance, &raster);
        RG_CHECKERROR(r);


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

        r = rgDrawFrame(instance, &frameInfo);
        RG_CHECKERROR(r);

        frameCount++;

        if (!toMove && frameCount > 0)
        {
            frameCount = 1;
        }
    }

    rgDestroyMaterial(instance, material);
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