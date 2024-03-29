// Copyright (c) 2022 Sultim Tsyrendashiev
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

#pragma once

#include "Common.h"

#include <filesystem>

struct cgltf_node;
struct cgltf_data;
struct cgltf_material;

namespace RTGL1
{

class Scene;
class TextureManager;
class TextureMetaManager;
class LightManager;

class GltfImporter
{
public:
    GltfImporter( const std::filesystem::path& gltfPath,
                  const RgTransform&           worldTransform,
                  float                        oneGameUnitInMeters );
    ~GltfImporter();

    GltfImporter( const GltfImporter& other )                = delete;
    GltfImporter( GltfImporter&& other ) noexcept            = delete;
    GltfImporter& operator=( const GltfImporter& other )     = delete;
    GltfImporter& operator=( GltfImporter&& other ) noexcept = delete;

    void UploadToScene( VkCommandBuffer           cmd,
                        uint32_t                  frameIndex,
                        Scene&                    scene,
                        TextureManager&           textureManager,
                        const TextureMetaManager& textureMeta ) const;

    explicit operator bool() const;

private:
    cgltf_data*           data;
    std::string           gltfPath;
    std::filesystem::path gltfFolder;
    float                 oneGameUnitInMeters;
};

}
