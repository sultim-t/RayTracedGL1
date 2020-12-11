#pragma once

#include "Common.h"
#include "VertexBufferManager.h"

class Scene
{
public:
    explicit Scene();

private:
    std::shared_ptr<VertexBufferManager> vertexBufferManager;

};