#include "Scene.h"

Scene::Scene(std::shared_ptr<ASManager> asManager)
{
    this->asManager = asManager;
}

void Scene::PrepareForFrame(uint32_t frameIndex)
{
    assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
    currentFrameIndex = frameIndex;

    asManager->BeginDynamicGeometry(frameIndex);
}

void Scene::SubmitForFrame(VkCommandBuffer cmd,  uint32_t frameIndex)
{
    asManager->SubmitDynamicGeometry(cmd, frameIndex);
}

uint32_t Scene::Upload(const RgGeometryUploadInfo &uploadInfo)
{
    if (uploadInfo.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        uint32_t geomId = asManager->AddDynamicGeometry(uploadInfo, currentFrameIndex);
    }
    else
    {
        uint32_t geomId = asManager->AddStaticGeometry(uploadInfo);
    }


}

void Scene::UpdateTransform(uint32_t geomId, const RgTransform &transform)
{
    asManager->UpdateStaticMovableTransform(geomId, transform);
}

void Scene::SubmitStatic()
{
}

void Scene::ClearStatic()
{
}

std::shared_ptr<ASManager> &Scene::GetASManager()
{
    return asManager;
}
