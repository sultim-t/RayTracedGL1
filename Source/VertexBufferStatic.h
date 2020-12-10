#pragma once
#include "VertexBuffer.h"

class VertexBufferStatic
{
public:
    VertexBufferStatic(VkDevice device, const PhysicalDevice &physDevice);
    ~VertexBufferStatic();

    void BeginCollecting();
    void EndCollecting();

    void Submit();
    void Reset();

private:
    VkDevice device;

};