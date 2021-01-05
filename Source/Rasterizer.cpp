#include "Rasterizer.h"

Rasterizer::Rasterizer(VkDevice device)
{
    this->device = device;
}

void Rasterizer::Upload(const RgRasterizedGeometryUploadInfo & uploadInfo)
{}

void Rasterizer::Draw(VkCommandBuffer cmd)
{}
