// Copyright (c) 2020-2021 Sultim Tsyrendashiev
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

#include <stdexcept>

#include "VulkanDevice.h"

using namespace RTGL1;


// TODO: check all members of input structs in RTGL1.CPP



#define MAX_DEVICE_COUNT 8
static std::map<RgInstance, std::shared_ptr<VulkanDevice>> g_Devices;


#define CHECK_WRONG_INSTANCE_AND_GET \
    auto &iterDevice = g_Devices.find(rgInstance); \
    if (iterDevice == g_Devices.end()) { return RG_WRONG_INSTANCE; } \
    iterDevice->second


#define CHECK_WRONG_INSTANCE_AND_RETURN_GET \
    auto &iterDevice = g_Devices.find(rgInstance); \
    if (iterDevice == g_Devices.end()) { return RG_WRONG_INSTANCE; } \
    return iterDevice->second


RgResult rgCreateInstance(const RgInstanceCreateInfo *info, RgInstance *pResult)
{
    if (g_Devices.size() >= MAX_DEVICE_COUNT)
    {
        *pResult = nullptr;
        return RG_TOO_MANY_INSTANCES;
    }


    {
        int count = 
            !!info->pWin32SurfaceInfo +
            !!info->pMetalSurfaceCreateInfo +
            !!info->pWaylandSurfaceCreateInfo +
            !!info->pXcbSurfaceCreateInfo +
            !!info->pXlibSurfaceCreateInfo;

        if (count != 1)
        {
            throw std::runtime_error("Exactly one of the surface infos must be specified");
        }
    }


    // insert new
    const RgInstance id = reinterpret_cast<RgInstance>(g_Devices.size() + 1);
    g_Devices[id] = std::make_shared<VulkanDevice>(info);

    *pResult = id;

    return RG_SUCCESS;
}


RgResult rgDestroyInstance(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_GET.reset();
    return RG_SUCCESS;
}

RgResult rgUploadGeometry(RgInstance rgInstance, const RgGeometryUploadInfo *pUploadInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadGeometry(pUploadInfo);
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* pUpdateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateGeometryTransform(pUpdateInfo);
}

RgResult rgUpdateGeometryTexCoords(RgInstance rgInstance, const RgUpdateTexCoordsInfo *pUpdateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateGeometryTexCoords(pUpdateInfo);
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, const RgRasterizedGeometryUploadInfo *pUploadInfo, 
                                    const float *pViewProjection, const RgViewport *pViewport)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadRasterizedGeometry(pUploadInfo, pViewProjection, pViewport);
}

RgResult rgSubmitStaticGeometries(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->SubmitStaticGeometries();
}

RgResult rgStartNewScene(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->StartNewStaticScene();
}

RgResult rgUploadDirectionalLight(RgInstance rgInstance, RgDirectionalLightUploadInfo *pLightInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadLight(pLightInfo);
}

RgResult rgUploadSphericalLight(RgInstance rgInstance, RgSphericalLightUploadInfo *pLightInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadLight(pLightInfo);
}

RgResult rgCreateStaticMaterial(RgInstance rgInstance, const RgStaticMaterialCreateInfo *pCreateInfo,
                               RgMaterial *pResult)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateStaticMaterial(pCreateInfo, pResult);
}

RgResult rgCreateAnimatedMaterial(RgInstance rgInstance, const RgAnimatedMaterialCreateInfo *pCreateInfo,
                                 RgMaterial *pResult)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateAnimatedMaterial(pCreateInfo, pResult);
}

RgResult rgChangeAnimatedMaterialFrame(RgInstance rgInstance, RgMaterial animatedMaterial, uint32_t frameIndex)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
}

RgResult rgCreateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialCreateInfo *pCreateInfo,
                                RgMaterial *pResult)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateDynamicMaterial(pCreateInfo, pResult);
}

RgResult rgUpdateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialUpdateInfo *pUpdateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateDynamicMaterial(pUpdateInfo);
}

RgResult rgDestroyMaterial(RgInstance rgInstance, RgMaterial material)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DestroyMaterial(material);
}

RgResult rgCreateCubemap(RgInstance rgInstance, const RgCubemapCreateInfo *pCreateInfo, RgCubemap *pResult)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateSkyboxCubemap(pCreateInfo, pResult);
}

RgResult rgDestroyCubemap(RgInstance rgInstance, RgCubemap cubemap)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DestroyCubemap(cubemap);
}

RgResult rgStartFrame(RgInstance rgInstance, const RgStartFrameInfo *pStartInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->StartFrame(pStartInfo);
}

RgResult rgDrawFrame(RgInstance rgInstance, const RgDrawFrameInfo *pDrawInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DrawFrame(pDrawInfo);
}
