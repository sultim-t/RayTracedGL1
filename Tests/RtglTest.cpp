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

#define RG_CHECKERROR(x) assert(x == RG_SUCCESS)
#define RG_CHECKERROR_R RG_CHECKERROR(r)

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


static glm::vec3 camPos = glm::vec3(0, 50, 0);
static glm::vec3 camDir = glm::vec3(0, 0, 1);
static glm::vec3 camUp = glm::vec3(0, 1, 0);
static glm::vec3 lightDir = glm::vec3(1, 1, 1);

void ProcessInput(GLFWwindow *window)
{
    float cameraSpeed = 60.0f / 60.0f;
    float cameraRotationSpeed = 5 / 60.0f;

    glm::vec3 r = glm::cross(camDir, glm::vec3(0, 1, 0));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camPos += cameraSpeed * camDir;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camPos -= cameraSpeed * camDir;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += r * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camPos -= glm::vec3(0, 1, 0) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camPos += glm::vec3(0, 1, 0) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camDir = glm::rotate(camDir, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camDir = glm::rotate(camDir, -cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camDir = glm::rotate(camDir, cameraRotationSpeed, r);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camDir = glm::rotate(camDir, -cameraRotationSpeed, r);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        lightDir = glm::rotate(lightDir, cameraRotationSpeed, glm::vec3(0, 1, 0));
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        lightDir = glm::rotate(lightDir, cameraRotationSpeed, glm::vec3(1, 0, 0));

    camUp = -glm::cross(r, camDir);
}

void LoadObj(const char *path, 
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

void StartScene(RgInstance instance, Window *pWindow)
{
    RgResult r;

    static std::vector<float> st_positions, dyn_positions;
    static std::vector<float> st_normals, dyn_normals;
    static std::vector<float> st_texCoords, dyn_texCoords;
    static std::vector<uint32_t> st_indices, dyn_indices;

    LoadObj("../../../BRUSHES.obj",
            st_positions,
            st_normals,
            st_texCoords,
            st_indices);

    LoadObj("../../../MODELS.obj",
            dyn_positions,
            dyn_normals,
            dyn_texCoords,
            dyn_indices);

    RgStaticMaterialCreateInfo matInfo[] = { {}, {} };

    matInfo[0].relativePath = "../../../TestImage.png";
    matInfo[0].useMipmaps = RG_TRUE;

    matInfo[1].relativePath = "../../../TestImage1.png";
    matInfo[1].useMipmaps = RG_TRUE;

    RgAnimatedMaterialCreateInfo animInfo = {};
    animInfo.frameCount = 2;
    animInfo.frames = matInfo;

    RgGeometryUploadInfo st_info = {};
    RgGeometryUploadInfo dyn_info = {};

    {
        st_info.geomType = RG_GEOMETRY_TYPE_STATIC;

        st_info.vertexCount = st_positions.size() / 3;
        st_info.vertexData = st_positions.data();
        st_info.normalData = st_normals.data();
        st_info.texCoordData = st_texCoords.data();

        st_info.indexCount = st_indices.size();
        st_info.indexData = st_indices.data();

        st_info.color = 0xFFFFFFFF;
        st_info.geomMaterial = {
            RG_NO_MATERIAL,
            RG_NO_MATERIAL,
            RG_NO_MATERIAL
        };

        st_info.transform = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0
        };
    }

    {
        dyn_info.geomType = RG_GEOMETRY_TYPE_DYNAMIC;
        dyn_info.passThroughType = RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE;

        dyn_info.vertexCount = dyn_positions.size() / 3;
        dyn_info.vertexData = dyn_positions.data();
        dyn_info.normalData = dyn_normals.data();
        dyn_info.texCoordData = dyn_texCoords.data();

        dyn_info.indexCount = dyn_indices.size();
        dyn_info.indexData = dyn_indices.data();

        dyn_info.color = 0xFFFFFFFF;
        dyn_info.geomMaterial = {
            RG_NO_MATERIAL,
            RG_NO_MATERIAL,
            RG_NO_MATERIAL
        };

        dyn_info.transform = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0
        };
    }


    /*st_info.geomType = RG_GEOMETRY_TYPE_STATIC_MOVABLE;
    st_info.transform = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0
    };
    RgGeometry movable;
    r = rgUploadGeometry(instance, &st_info, &movable);
    RG_CHECKERROR_R;*/


    uint64_t frameCount = 0;

    RgMaterial mat = RG_NO_MATERIAL;

    while (!glfwWindowShouldClose(pWindow->glfwHandle))
    {
        glfwPollEvents();
        pWindow->UpdateSize();
        ProcessInput(pWindow->glfwHandle);

        r = rgStartFrame(instance, (uint32_t)pWindow->width, (uint32_t)pWindow->height, true);
        RG_CHECKERROR_R;

        if (frameCount == 0)
        {
            r = rgStartNewScene(instance);
            RG_CHECKERROR_R;

            r = rgCreateAnimatedMaterial(instance, &animInfo, &mat);
            RG_CHECKERROR_R;



            r = rgUploadGeometry(instance, &st_info, nullptr);
            RG_CHECKERROR_R;

            r = rgSubmitStaticGeometries(instance);
            RG_CHECKERROR_R;
        }

        rgChangeAnimatedMaterialFrame(instance, mat, frameCount % 120 > 60);

        /*st_info.geomType = RG_GEOMETRY_TYPE_DYNAMIC;
        st_info.transform = {
            1,0,0,0,
            0,1,0, -static_cast<float>(frameCount) * 0.05f + 5,
            0,0,1,0
        };
        rgUploadGeometry(instance, &st_info, nullptr);*/

        dyn_info.geomMaterial.layerMaterials[0] = mat;

        rgUploadGeometry(instance, &dyn_info, nullptr);

        float verts[] = 
        {
            0, 0, 0,
            0, 0.5f, 0,
            0.5f, 0, 0,
            0.5f, 0, 0,
            0, 0.5f, 0,
            0.5f, 0.5f, 0
        };

        float texCoords[] =
        {
            0, 0, 
            0, 1.0f,
            1.0f, 0, 
            1.0f, 0, 
            0, 1.0f, 
            1.0, 1.0f,
        };

        uint32_t colors[]
        {
            0xFFFF0000,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFF00FF00,
        };

        RgRasterizedGeometryUploadInfo raster = {};
        raster.vertexData = verts;
        raster.vertexCount = 6;
        raster.vertexStride = 3 * sizeof(float);
        raster.colorData = colors;
        raster.colorStride = sizeof(uint32_t);
        raster.texCoordData = texCoords;
        raster.texCoordStride = 2 * sizeof(float);
        raster.textures = 
        {
            mat,
            RG_NO_MATERIAL,
            RG_NO_MATERIAL
        };

        float idnt[] =
        {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        memcpy(raster.viewProjection, idnt, 16 * sizeof(float));

        raster.viewport.x = -800;
        raster.viewport.y = -450;
        raster.viewport.width = 1600;
        raster.viewport.height = 900;

        //r = rgUploadRasterizedGeometry(instance, &raster);
        //RG_CHECKERROR_R;

        /*RgUpdateTransformInfo uptr = {};
        uptr.movableStaticGeom = movable;
        uptr.transform = {
            1,0,0,0,
            0,1,0, -static_cast<float>(frameCount) * 0.05f + 30,
            0,0,1,0
        };
        r = rgUpdateGeometryTransform(instance, &uptr);
        RG_CHECKERROR_R;*/

        glm::mat4 persp = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 10000.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos + camDir, camUp);

        RgDrawFrameInfo frameInfo = {};
        frameInfo.renderWidth = pWindow->width;
        frameInfo.renderHeight = pWindow->height;
        // GLM is column major, copy matrix data directly
        memcpy(frameInfo.view, &view[0][0], 16 * sizeof(float));
        memcpy(frameInfo.projection, &persp[0][0], 16 * sizeof(float));

        r = rgDrawFrame(instance, &frameInfo);
        RG_CHECKERROR_R;

        frameCount++;
    }

    rgDestroyMaterial(instance, mat);
}

int main()
{
    static Window window = Window();

    logFile.open("LogOutput.txt");

    try
    {
        RgInstanceCreateInfo info = {};
        info.name = "RTGL1 Test";
        info.physicalDeviceIndex = 0;
        info.enableValidationLayer = RG_TRUE;

        info.vertexPositionStride = 3 * sizeof(float);
        info.vertexNormalStride = 3 * sizeof(float);
        info.vertexTexCoordStride = 2 * sizeof(float);
        info.vertexColorStride = sizeof(uint32_t);
        info.rasterizedMaxVertexCount = 4096;
        info.rasterizedMaxIndexCount = 2048;

        info.ppWindowExtensions = window.extensions;
        info.windowExtensionCount = window.extensionCount;

        info.pfnCreateSurface = [](uint64_t vkInstance, uint64_t *pResultVkSurfaceKHR)
        {
            window.CreateVkSurface(vkInstance, pResultVkSurfaceKHR);
        };

        info.pfnDebugPrint = DebugPrint;

        RgInstance instance;
        rgCreateInstance(&info, &instance);

        StartScene(instance, &window);

        rgDestroyInstance(instance);
    }
    catch(std::exception &e)
    {
        logFile << e.what() << '\n';
    }

    logFile.close();
}