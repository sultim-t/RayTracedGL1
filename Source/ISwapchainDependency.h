#pragma once

class ISwapchainDependency
{
public:
    virtual ~ISwapchainDependency() = default;
    virtual void OnSwapchainCreate(uint32_t newWidth, uint32_t newHeight) = 0;
    virtual void OnSwapchainDestroy() = 0;
};

