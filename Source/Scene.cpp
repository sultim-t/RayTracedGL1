#include "Scene.h"

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

void Scene::SubmitForFrame(VkCommandBuffer cmd,  uint32_t frameIndex)
{
    if (toResubmitMovable)
    {
        // at least one transform of static movable geometry was changed
        asManager->ResubmitStaticMovable(cmd);
    }

    // always submit dynamic geomtetry on the frame ending
    asManager->SubmitDynamicGeometry(cmd, frameIndex);

    // reset
    currentFrameIndex = UINT32_MAX;
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

void Scene::UpdateTransform(uint32_t geomId, const RgTransform &transform)
{
    // check if it's actually movable
    if (std::find(movableGeomIds.begin(), movableGeomIds.end(), geomId) != movableGeomIds.end())
    {
        // do nothing, if it's not
        return;
    }

    asManager->UpdateStaticMovableTransform(geomId, transform);

    // if not recording, then static geometries were already submitted,
    // as some movable transform was changed AS must be rebuilt
    if (!isRecordingStatic)
    {
        toResubmitMovable = true;
    }
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

std::shared_ptr<ASManager> &Scene::GetASManager()
{
    return asManager;
}
