#pragma once
#include "Common.h"

class Swapchain;

class ISwapchainDependency
{
public:
    virtual ~ISwapchainDependency() = default;
    virtual void OnSwapchainCreate(const Swapchain *pSwapchain) = 0;
    virtual void OnSwapchainDestroy() = 0;
};

