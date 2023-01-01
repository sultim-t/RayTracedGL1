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
#include "Containers.h"
#include "IFileDependency.h"

#include <array>
#include <string>

namespace RTGL1
{

struct TextureMeta
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::string textureName = {};

    bool forceIgnore      = false;
    bool forceAlphaTest   = false;
    bool forceTranslucent = false;

    bool isMirror             = false;
    bool isWater              = false;
    bool isGlass              = false;
    bool isGlassIfTranslucent = false;

    float metallicDefault  = 0.0f;
    float roughnessDefault = 1.0f;
    float emissiveMult     = 0.0f;

    float                    attachedLightIntensity = 0.0f;
    std::array< uint8_t, 3 > attachedLightColor     = { { 255, 255, 255 } };
};

struct TextureMetaArray
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::vector< TextureMeta > array;
};


class TextureMetaManager : public IFileDependency
{
public:
    explicit TextureMetaManager( std::filesystem::path databaseFolder );
    ~TextureMetaManager() override = default;

    TextureMetaManager( const TextureMetaManager& other )                = delete;
    TextureMetaManager( TextureMetaManager&& other ) noexcept            = delete;
    TextureMetaManager& operator=( const TextureMetaManager& other )     = delete;
    TextureMetaManager& operator=( TextureMetaManager&& other ) noexcept = delete;

    void Modify( RgMeshPrimitiveInfo& prim, RgEditorInfo& editor, bool isStatic ) const;

    void RereadFromFiles( std::string_view currentSceneName );
    void OnFileChanged( FileType type, const std::filesystem::path& filepath ) override;

private:
    std::optional< TextureMeta > Access( const char* pTextureName ) const;
    void                         RereadFromFiles( std::filesystem::path sceneFile );

private:
    std::filesystem::path databaseFolder;

    std::filesystem::path sourceGlobal;
    std::filesystem::path sourceScene;

    rgl::unordered_map< std::string, TextureMeta > dataGlobal;
    rgl::unordered_map< std::string, TextureMeta > dataScene;
};

}