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

#include "VulkanDevice.h"


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


RgResult rgCreateInstance(const RgInstanceCreateInfo *info, RgInstance *result)
{
    if (g_Devices.size() >= MAX_DEVICE_COUNT)
    {
        *result = nullptr;
        return RG_TOO_MANY_INSTANCES;
    }

    // insert new
    const RgInstance id = reinterpret_cast<RgInstance>(g_Devices.size() + 1);
    g_Devices[id] = std::make_shared<VulkanDevice>(info);

    *result = id;

    return RG_SUCCESS;
}


RgResult rgDestroyInstance(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_GET.reset();
    return RG_SUCCESS;
}

RgResult rgUploadGeometry(RgInstance rgInstance, const RgGeometryUploadInfo *uploadInfo, RgGeometry *result)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadGeometry(uploadInfo, result);
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* updateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateGeometryTransform(updateInfo);
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, const RgRasterizedGeometryUploadInfo *uploadInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadRasterizedGeometry(uploadInfo);
}

RgResult rgSubmitStaticGeometries(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->SubmitStaticGeometries();
}

RgResult rgStartNewScene(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->StartNewStaticScene();
}

RgResult rgCreateStaticMaterial(RgInstance rgInstance, const RgStaticMaterialCreateInfo *createInfo,
                               RgMaterial *result)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateStaticMaterial(createInfo, result);
}

RgResult rgCreateAnimatedMaterial(RgInstance rgInstance, const RgAnimatedMaterialCreateInfo *createInfo,
                                 RgMaterial *result)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateAnimatedMaterial(createInfo, result);
}

RgResult rgChangeAnimatedMaterialFrame(RgInstance rgInstance, RgMaterial animatedMaterial, uint32_t frameIndex)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
}

RgResult rgCreateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialCreateInfo *createInfo,
                                RgMaterial *result)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateDynamicMaterial(createInfo, result);
}

RgResult rgUpdateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialUpdateInfo *updateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateDynamicMaterial(updateInfo);
}

RgResult rgDestroyMaterial(RgInstance rgInstance, RgMaterial material)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DestroyMaterial(material);
}

RgResult rgStartFrame(RgInstance rgInstance, uint32_t surfaceWidth, uint32_t surfaceHeight, RgBool32 vsync)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->StartFrame(surfaceWidth, surfaceHeight, vsync);
}

RgResult rgDrawFrame(RgInstance rgInstance, const RgDrawFrameInfo *frameInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DrawFrame(frameInfo);
}
