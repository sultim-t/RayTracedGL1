// Copyright (c) 2020-2021 Sultim Tsyrendashiev
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

#include "ASManager.h"
#include "GltfExporter.h"
#include "GltfImporter.h"
#include "LightManager.h"
#include "VertexPreprocessing.h"
#include "TextureMeta.h"

#include <variant>

namespace RTGL1
{

enum class UploadResult
{
    Fail,
    Static,
    Dynamic,
    ExportableDynamic,
    ExportableStatic,
};

class Scene
{
public:
    explicit Scene( VkDevice                                device,
                    const PhysicalDevice&                   physDevice,
                    std::shared_ptr< MemoryAllocator >&     allocator,
                    std::shared_ptr< CommandBufferManager > cmdManager,
                    const GlobalUniform&                    uniform,
                    const ShaderManager&                    shaderManager,
                    bool                                    enableTexCoordLayer1,
                    bool                                    enableTexCoordLayer2,
                    bool                                    enableTexCoordLayer3 );
    ~Scene() = default;

    Scene( const Scene& other )                = delete;
    Scene( Scene&& other ) noexcept            = delete;
    Scene& operator=( const Scene& other )     = delete;
    Scene& operator=( Scene&& other ) noexcept = delete;

    void PrepareForFrame( VkCommandBuffer cmd, uint32_t frameIndex, bool ignoreExternalGeometry );
    void SubmitForFrame( VkCommandBuffer                         cmd,
                         uint32_t                                frameIndex,
                         const std::shared_ptr< GlobalUniform >& uniform,
                         uint32_t                                uniformData_rayCullMaskWorld,
                         bool                                    allowGeometryWithSkyFlag,
                         bool                                    disableRTGeometry );

    UploadResult UploadPrimitive( uint32_t                   frameIndex,
                                  const RgMeshInfo&          mesh,
                                  const RgMeshPrimitiveInfo& primitive,
                                  const TextureManager&      textureManager,
                                  bool                       isStatic );

    UploadResult UploadLight( uint32_t               frameIndex,
                              const GenericLightPtr& light,
                              LightManager*          lightManager,
                              bool                   isStatic );

    void SubmitStaticLights( uint32_t frameIndex, LightManager& lightManager ) const;

    void NewScene( VkCommandBuffer           cmd,
                   uint32_t                  frameIndex,
                   const GltfImporter&       staticScene,
                   TextureManager&           textureManager,
                   const TextureMetaManager& textureMeta );

    const std::shared_ptr< ASManager >&           GetASManager();
    const std::shared_ptr< VertexPreprocessing >& GetVertexPreprocessing();

    std::optional< uint64_t > TryGetVolumetricLight() const;

private:
    bool StaticMeshExists( const RgMeshInfo& mesh ) const;
    bool StaticLightExists( const GenericLightPtr& light ) const;

    bool InsertPrimitiveInfo( uint64_t                   uniqueID,
                              bool                       isStatic,
                              const RgMeshInfo&          mesh,
                              const RgMeshPrimitiveInfo& primitive );

    bool InsertLightInfo( bool isStatic, const GenericLightPtr& light );

private:
    std::shared_ptr< ASManager >           asManager;
    std::shared_ptr< GeomInfoManager >     geomInfoMgr;
    std::shared_ptr< VertexPreprocessing > vertPreproc;

    // Dynamic indices are cleared every frame
    rgl::unordered_set< uint64_t >    dynamicUniqueIDs;
    rgl::unordered_set< uint64_t >    staticUniqueIDs;
    rgl::unordered_set< std::string > staticMeshNames;
    std::vector< GenericLight >       staticLights;

    StaticGeometryToken  makingStatic{};
    DynamicGeometryToken makingDynamic{};

    bool ignoreExternalGeometry{};
};


class SceneImportExport : public IFileDependency
{
public:
    SceneImportExport( std::filesystem::path _scenesFolder,
                       const RgFloat3D&      _worldUp,
                       const RgFloat3D&      _worldForward,
                       const float&          _worldScale );
    ~SceneImportExport() override = default;

    SceneImportExport( const SceneImportExport& other )                = delete;
    SceneImportExport( SceneImportExport&& other ) noexcept            = delete;
    SceneImportExport& operator=( const SceneImportExport& other )     = delete;
    SceneImportExport& operator=( SceneImportExport&& other ) noexcept = delete;

    void PrepareForFrame();
    void CheckForNewScene( std::string_view    mapName,
                           VkCommandBuffer     cmd,
                           uint32_t            frameIndex,
                           Scene&              scene,
                           TextureManager&     textureManager,
                           TextureMetaManager& textureMetaManager );
    void TryExport( TextureManager& textureManager );

    void RequestReimport();
    void OnFileChanged( FileType type, const std::filesystem::path& filepath ) override;

    void          RequestExport();
    GltfExporter* TryGetExporter();

    std::string_view GetImportMapName() const;
    std::string_view GetExportMapName() const;

    const RgFloat3D& GetWorldUp() const;
    const RgFloat3D& GetWorldForward() const;
    float            GetWorldScale() const;

    std::filesystem::path MakeGltfPath( std::string_view mapName );
    RgTransform           MakeWorldTransform() const;

private:
    std::filesystem::path scenesFolder;

    bool reimportRequested{ false };

    bool                            exportRequested{ false };
    std::unique_ptr< GltfExporter > exporter{};

    std::string currentMap{};
    RgFloat3D   worldUp;
    RgFloat3D   worldForward;
    float       worldScale;

public:
    mutable struct
    {
        struct DevField
        {
            bool enable{ false };
            char value[ 128 ]{ "" };

            void SetDefaults( const SceneImportExport& s )
            {
                std::snprintf( value, std::size( value ), "%s", s.currentMap.c_str() );
                value[ std::size( value ) - 1 ] = '\0';
            }
        };

        DevField importName;
        DevField exportName;

        struct
        {
            bool      enable{ false };
            RgFloat3D up{};
            RgFloat3D forward{};
            float     scale{};

            void SetDefaults( const SceneImportExport& s )
            {
                up      = s.worldUp;
                forward = s.worldForward;
                scale   = s.worldScale;
            }
        } worldTransform;
    } dev;
};

}