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

#include <RTGL1/RTGL1.h>

#include "Containers.h"
#include "TextureOverrides.h"

namespace RTGL1
{
    class TextureManager;


    class TextureObserver
    {
    public:
        TextureObserver() = default;
        ~TextureObserver() = default;

        TextureObserver(const TextureObserver& other) = delete;
        TextureObserver(TextureObserver&& other) noexcept = delete;
        TextureObserver& operator=(const TextureObserver& other) = delete;
        TextureObserver& operator=(TextureObserver&& other) noexcept = delete;

        void CheckPathsAndReupload(VkCommandBuffer cmd, TextureManager &manager, ImageLoaderDev *loader);

        void RegisterPath(RgMaterial index, std::optional<std::filesystem::path> path, const std::optional<ImageLoader::ResultInfo> &imageInfo, uint32_t textureType);
        void Remove(RgMaterial index);

    private:
        struct DependentFile
        {
            std::filesystem::path path;
            std::filesystem::file_time_type lastWriteTime;
            size_t dataSize;
            VkFormat format;
            uint32_t textureType;
        };

        static bool HaveChanged(std::vector<DependentFile> &files);

    private:
        rgl::unordered_map<RgMaterial, std::vector<DependentFile>> materials;
        std::chrono::time_point<std::chrono::system_clock> lastCheck;
    };
}