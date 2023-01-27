// Copyright (c) 2023 Sultim Tsyrendashiev
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
#include "Containers.h"
#include "IFileDependency.h"
#include "JsonParser.h"

#include <string>

namespace RTGL1
{

class SceneMetaManager final : public IFileDependency
{
public:
    explicit SceneMetaManager( std::filesystem::path filepath );
    ~SceneMetaManager() override = default;

    SceneMetaManager( const SceneMetaManager& other )                = delete;
    SceneMetaManager( SceneMetaManager&& other ) noexcept            = delete;
    SceneMetaManager& operator=( const SceneMetaManager& other )     = delete;
    SceneMetaManager& operator=( SceneMetaManager&& other ) noexcept = delete;

    void Modify( std::string_view             sceneName,
                 RgDrawFrameVolumetricParams& volumetric,
                 RgDrawFrameSkyParams&        sky ) const;

    void OnFileChanged( FileType type, const std::filesystem::path& filepath ) override;
    
private:
    std::filesystem::path metafile;

    rgl::unordered_map< std::string, SceneMeta > data;
};

}