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

static void DebugPrint(const char *msg)
{
    std::cout << msg;
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


static std::vector<float> _positions;
static std::vector<float> _normals;
static std::vector<float> _texCoords;
static std::vector<uint32_t> _colors;
static std::vector<uint32_t> _indices;

void LoadObj(const char *path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);
    assert(ret);

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

                _colors.push_back(0xFFFFFFFF);

                _indices.push_back(index_offset + v);
            }

            index_offset += fv;
        }
    }

}

void StartScene(RgInstance instance, Window *pWindow)
{
    LoadObj("../../BRUSHES.obj");

    RgGeometryUploadInfo info = {};
    info.geomType = RG_GEOMETRY_TYPE_STATIC;

    info.vertexCount = _positions.size();
    info.vertexData = _positions.data();
    info.normalData = _normals.data();
    info.texCoordData = _texCoords.data();
    info.colorData = _colors.data();

    info.indexCount = _indices.size();
    info.indexData = _indices.data();

    info.geomMaterial = {
        RG_NO_TEXTURE,
        RG_NO_TEXTURE,
        RG_NO_TEXTURE
    };

    info.triangleMaterials = nullptr;

    info.transform = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0
    };

    rgStartNewScene(instance);

    RgGeometry geom;
    rgUploadGeometry(instance, &info, &geom);

    rgSubmitStaticGeometries(instance);

    while (!glfwWindowShouldClose(pWindow->glfwHandle))
    {
        glfwPollEvents();
        pWindow->UpdateSize();
        ProcessInput(pWindow->glfwHandle);

        rgStartFrame(instance);

        RgDrawFrameInfo frameInfo = {};
        frameInfo.width = pWindow->width;
        frameInfo.height = pWindow->height;
        

        rgDrawFrame(instance, &frameInfo);

    }

}

int main()
{
    static Window window = Window();

    RgInstanceCreateInfo info = {};
    info.name = "RTGL1 Test";
    info.physicalDeviceIndex = 0;
    info.enableValidationLayer = RG_TRUE;

    info.vertexPositionStride = 3 * sizeof(float);
    info.vertexNormalStride = 3 * sizeof(float);
    info.vertexTexCoordStride = 2 * sizeof(float);
    info.vertexColorStride = 3 * sizeof(float);
    info.rasterizedDataBufferSize = 32 * 1024 * 1024;

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
}