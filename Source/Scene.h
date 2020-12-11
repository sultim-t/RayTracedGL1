#pragma once

#include "Common.h"
#include "ASManager.h"

class Scene
{
public:
    explicit Scene();

private:
    std::shared_ptr<ASManager> asManager;

};