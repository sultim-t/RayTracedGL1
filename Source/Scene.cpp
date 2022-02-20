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

#include "Scene.h"
#include "Generated/ShaderCommonC.h"
#include "RgException.h"
#include "CmdLabel.h"

using namespace RTGL1;

Scene::Scene(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> &_allocator,
    std::shared_ptr<CommandBufferManager> &_cmdManager,
    std::shared_ptr<TextureManager> &_textureManager,
    const std::shared_ptr<const GlobalUniform> &_uniform,
    const std::shared_ptr<const ShaderManager> &_shaderManager,
    const VertexBufferProperties &_properties)
:
    toResubmitMovable(false),
    isRecordingStatic(false),
    submittedStaticInCurrentFrame(false)
{
    VertexCollectorFilterTypeFlags_Init();

    sectorVisibility = std::make_shared<SectorVisibility>();

    lightManager = std::make_shared<LightManager>(_device, _allocator, sectorVisibility);
    geomInfoMgr = std::make_shared<GeomInfoManager>(_device, _allocator);
    triangleInfoMgr = std::make_shared<TriangleInfoManager>(_device, _allocator, sectorVisibility);

    asManager = std::make_shared<ASManager>(_device, _allocator, _cmdManager, _textureManager, geomInfoMgr, triangleInfoMgr, sectorVisibility, _properties);
  
    vertPreproc = std::make_shared<VertexPreprocessing>(_device, _uniform, asManager, _shaderManager);
}

Scene::~Scene()
{}

void Scene::PrepareForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    dynamicUniqueIDToSimpleIndex.clear();

    geomInfoMgr->PrepareForFrame(frameIndex);
    triangleInfoMgr->PrepareForFrame(frameIndex);
    lightManager->PrepareForFrame(cmd, frameIndex);

    // dynamic geomtry
    asManager->BeginDynamicGeometry(cmd, frameIndex);
}

bool Scene::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, 
                           uint32_t uniformData_rayCullMaskWorld, bool allowGeometryWithSkyFlag, bool isReflRefrAlphaTested, bool disableRayTracing)
{
    uint32_t preprocMode = submittedStaticInCurrentFrame ? VERT_PREPROC_MODE_ALL : 
                           toResubmitMovable             ? VERT_PREPROC_MODE_DYNAMIC_AND_MOVABLE : 
                                                           VERT_PREPROC_MODE_ONLY_DYNAMIC;
    submittedStaticInCurrentFrame = false;


    lightManager->CopyFromStaging(cmd, frameIndex);


    // copy to device-local, if there were any tex coords change for static geometry
    asManager->ResubmitStaticTexCoords(cmd);

    if (toResubmitMovable)
    {
        // at least one transform of static movable geometry was changed
        asManager->ResubmitStaticMovable(cmd);
        toResubmitMovable = false;
    }

    // always submit dynamic geomtetry on the frame ending
    asManager->SubmitDynamicGeometry(cmd, frameIndex);


    // copy geom and tri infos to device-local
    geomInfoMgr->CopyFromStaging(cmd, frameIndex);
    triangleInfoMgr->CopyFromStaging(cmd, frameIndex);


    ShVertPreprocessing push = {};
    ASManager::TLASPrepareResult prepare = {};

    asManager->PrepareForBuildingTLAS(frameIndex, *uniform->GetData(), uniformData_rayCullMaskWorld, allowGeometryWithSkyFlag, isReflRefrAlphaTested, &push, &prepare);

    // upload uniform data
    uniform->GetData()->areFramebufsInitedByRT = !prepare.IsEmpty() && !disableRayTracing;
    uniform->Upload(cmd, frameIndex);
    
    
    vertPreproc->Preprocess(cmd, frameIndex, preprocMode, uniform, asManager, push);


    if (prepare.IsEmpty())
    {
        return false;
    }


    asManager->BuildTLAS(cmd, frameIndex, prepare);
    return true;
}

bool Scene::Upload(uint32_t frameIndex, const RgGeometryUploadInfo &uploadInfo)
{
    assert(!DoesUniqueIDExist(uploadInfo.uniqueID));

    if (uploadInfo.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        if (isRecordingStatic)
        {
            throw RgException(RG_WRONG_FUNCTION_CALL, "Dynamic geometry must not be uploaded between rgStartNewScene and rgSubmitStaticGeometries calls");
        }

        uint32_t simpleIndex = asManager->AddDynamicGeometry(frameIndex, uploadInfo);

        if (simpleIndex != UINT32_MAX)
        {
            dynamicUniqueIDToSimpleIndex[uploadInfo.uniqueID] = simpleIndex;
            return true;
        }
    }
    else
    {
        if (!isRecordingStatic)
        {          
            // never allow submitting static geometry out of StartNewStatic-SubmitStatic
            throw RgException(RG_WRONG_FUNCTION_CALL, "Submitting static geometry is only allowed between rgStartNewScene and rgSubmitStaticGeometries calls");
        }

        uint32_t simpleIndex = asManager->AddStaticGeometry(frameIndex, uploadInfo);

        if (simpleIndex != UINT32_MAX)
        {
            staticUniqueIDToSimpleIndex[uploadInfo.uniqueID] = simpleIndex;

            if (uploadInfo.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
            {
                movableGeomIndices.push_back(simpleIndex);
            }

            return true;
        }
    }

    return false;
}

bool Scene::UpdateTransform(const RgUpdateTransformInfo &updateInfo)
{
    uint32_t simpleIndex;
    if (!TryGetStaticSimpleIndex(updateInfo.movableStaticUniqueID, &simpleIndex))
    {
        throw RgException(RG_CANT_UPDATE_TRANSFORM, "Can't find static geometry with unique ID=" + std::to_string(updateInfo.movableStaticUniqueID));
    }

    // check if it's actually movable
    if (std::find(movableGeomIndices.begin(), movableGeomIndices.end(), simpleIndex) == movableGeomIndices.end())
    {
        throw RgException(RG_CANT_UPDATE_TRANSFORM, "Static geometry with unique ID=" + std::to_string(updateInfo.movableStaticUniqueID) + " isn't movable");
    }

    asManager->UpdateStaticMovableTransform(simpleIndex, updateInfo);

    // if not recording, then static geometries were already submitted,
    // as some movable transform was changed AS must be rebuilt
    if (!isRecordingStatic)
    {
        toResubmitMovable = true;
    }

    return true;
}

bool RTGL1::Scene::UpdateTexCoords(const RgUpdateTexCoordsInfo &texCoordsInfo)
{
    uint32_t simpleIndex;
    if (!TryGetStaticSimpleIndex(texCoordsInfo.staticUniqueID, &simpleIndex))
    {
        throw RgException(RG_CANT_UPDATE_TEXCOORDS, "Can't find static geometry with unique ID=" + std::to_string(texCoordsInfo.staticUniqueID));
    }

    asManager->UpdateStaticTexCoords(simpleIndex, texCoordsInfo);
    return true;
}

void Scene::SubmitStatic()
{
    // submit even if nothing was recorded, 
    // so the static scene will be empty
    if (!isRecordingStatic)
    {
        asManager->BeginStaticGeometry();
    }

    asManager->SubmitStaticGeometry();
    isRecordingStatic = false;

    submittedStaticInCurrentFrame = true;
}

void Scene::StartNewStatic()
{
    if (isRecordingStatic)
    {
        throw RgException(RG_WRONG_FUNCTION_CALL, "rgStartNewScene must be called only once before rgSubmitStaticGeometries");
    }

    isRecordingStatic = true;
    asManager->BeginStaticGeometry();
    lightManager->Reset();
    sectorVisibility->Reset();

    staticUniqueIDToSimpleIndex.clear();
    movableGeomIndices.clear();
}

const std::shared_ptr<ASManager> &Scene::GetASManager()
{
    return asManager;
}

const std::shared_ptr<LightManager> &RTGL1::Scene::GetLightManager()
{
    return lightManager;
}

const std::shared_ptr<VertexPreprocessing> &RTGL1::Scene::GetVertexPreprocessing()
{
    return vertPreproc;
}

bool Scene::DoesUniqueIDExist(uint64_t uniqueID) const
{
    return
        staticUniqueIDToSimpleIndex.find(uniqueID) != staticUniqueIDToSimpleIndex.end() ||
        dynamicUniqueIDToSimpleIndex.find(uniqueID) != dynamicUniqueIDToSimpleIndex.end();
}

bool Scene::TryGetStaticSimpleIndex(uint64_t uniqueID, uint32_t *result) const
{
    auto f = staticUniqueIDToSimpleIndex.find(uniqueID);

    if (f != staticUniqueIDToSimpleIndex.end())
    {
        *result = f->second;
        return true;
    }

    return false;
}

void Scene::UploadLight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgDirectionalLightUploadInfo &lightInfo)
{
    lightManager->AddDirectionalLight(frameIndex, uniform, lightInfo);
}

void Scene::UploadLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &lightInfo)
{
    lightManager->AddSphericalLight(frameIndex, lightInfo);
}

void RTGL1::Scene::UploadLight(uint32_t frameIndex, const RgPolygonalLightUploadInfo &lightInfo)
{
    lightManager->AddPolygonalLight(frameIndex, lightInfo);
}

void Scene::UploadLight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgSpotlightUploadInfo &lightInfo)
{
    lightManager->AddSpotlight(frameIndex, uniform, lightInfo);
}

void RTGL1::Scene::SetPotentialVisibility(SectorID sectorID_A, SectorID sectorID_B)
{
    sectorVisibility->SetPotentialVisibility(sectorID_A, sectorID_B);
}
