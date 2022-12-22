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
#include "TextureManager.h"

#include <filesystem>
#include <functional>
#include <set>

namespace RTGL1
{
struct DeepCopyOfPrimitive;

struct GltfMeshNode
{
    std::string name;
    RgTransform transform;

    bool     operator==( const GltfMeshNode& other ) const;
    bool     operator<( const GltfMeshNode& other ) const;
    uint64_t Hash() const;
};
}

template<>
struct std::hash< RTGL1::GltfMeshNode >
{
    std::size_t operator()( RTGL1::GltfMeshNode const& m ) const noexcept { return m.Hash(); }
};

namespace RTGL1
{

using MeshesToTheirPrimitives =
    rgl::unordered_map< GltfMeshNode, std::vector< std::shared_ptr< DeepCopyOfPrimitive > > >;

class GltfExporter
{
public:
    explicit GltfExporter( const RgTransform& worldTransform );
    ~GltfExporter() = default;

    GltfExporter( const GltfExporter& other )                = delete;
    GltfExporter( GltfExporter&& other ) noexcept            = delete;
    GltfExporter& operator=( const GltfExporter& other )     = delete;
    GltfExporter& operator=( GltfExporter&& other ) noexcept = delete;

    void AddPrimitive( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive );

    void ExportToFiles( const std::filesystem::path& gltfPath,
                        const TextureManager&        textureManager );

private:
    MeshesToTheirPrimitives scene;
    std::set< std::string > sceneMaterials;

    RgTransform worldTransform;
};
}
