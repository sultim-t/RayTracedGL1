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

using namespace RTGL1;

Scene::Scene(std::shared_ptr<ASManager> asManager) :
    toResubmitMovable(false),
    currentFrameIndex(UINT32_MAX),
    isRecordingStatic(false)
{
    this->asManager = asManager;
}

Scene::~Scene()
{}

void Scene::PrepareForFrame(uint32_t frameIndex)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    currentFrameIndex = frameIndex;

    // dynamic geomtry
    asManager->BeginDynamicGeometry(frameIndex);
}

bool Scene::SubmitForFrame(VkCommandBuffer cmd,  uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform)
{
    // reset
    currentFrameIndex = UINT32_MAX;

    if (toResubmitMovable)
    {
        // at least one transform of static movable geometry was changed
        asManager->ResubmitStaticMovable(cmd);
        toResubmitMovable = false;
    }

    // always submit dynamic geomtetry on the frame ending
    asManager->SubmitDynamicGeometry(cmd, frameIndex);

    // try to build top level
    return asManager->TryBuildTLAS(cmd, frameIndex, uniform);
}

uint32_t Scene::Upload(const RgGeometryUploadInfo &uploadInfo)
{
    if (uploadInfo.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        return asManager->AddDynamicGeometry(uploadInfo, currentFrameIndex);
    }
    else
    {
        if (!isRecordingStatic)
        {
            asManager->BeginStaticGeometry();
            isRecordingStatic = true;
        }

        uint32_t geomId = asManager->AddStaticGeometry(uploadInfo);

        if (uploadInfo.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
        {
            movableGeomIds.push_back(geomId);
        }

        return geomId;
    }
}

bool Scene::UpdateTransform(uint32_t geomId, const RgTransform &transform)
{
    // check if it's actually movable
    if (std::find(movableGeomIds.begin(), movableGeomIds.end(), geomId) == movableGeomIds.end())
    {
        // do nothing, if it's not
        return false;
    }

    asManager->UpdateStaticMovableTransform(geomId, transform);

    // if not recording, then static geometries were already submitted,
    // as some movable transform was changed AS must be rebuilt
    if (!isRecordingStatic)
    {
        toResubmitMovable = true;
    }

    return true;
}

void Scene::SubmitStatic()
{
    // submit even if nothing was recorded
    if (!isRecordingStatic)
    {
        asManager->BeginStaticGeometry();
    }

    asManager->SubmitStaticGeometry();
    isRecordingStatic = false;
}

void Scene::StartNewStatic()
{
    // if static geometry wasn't submitted yet
    if (isRecordingStatic)
    {
        // then just reset it
        asManager->ResetStaticGeometry();
    }

    movableGeomIds.clear();
}

bool Scene::IsRecordingStatic() const
{
    return isRecordingStatic;
}

std::shared_ptr<ASManager> &Scene::GetASManager()
{
    return asManager;
}
