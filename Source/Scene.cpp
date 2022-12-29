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

#include "Scene.h"

#include "CmdLabel.h"
#include "GeomInfoManager.h"
#include "GltfImporter.h"
#include "RgException.h"
#include "UniqueID.h"

#include "Generated/ShaderCommonC.h"

RTGL1::Scene::Scene( VkDevice                                _device,
                     const PhysicalDevice&                   _physDevice,
                     std::shared_ptr< MemoryAllocator >&     _allocator,
                     std::shared_ptr< CommandBufferManager > _cmdManager,
                     const GlobalUniform&                    _uniform,
                     const ShaderManager&                    _shaderManager )
{
    VertexCollectorFilterTypeFlags_Init();

    geomInfoMgr = std::make_shared< GeomInfoManager >( _device, _allocator );

    asManager = std::make_shared< ASManager >(
        _device, _physDevice, _allocator, std::move( _cmdManager ), geomInfoMgr );

    vertPreproc =
        std::make_shared< VertexPreprocessing >( _device, _uniform, *asManager, _shaderManager );
}

void RTGL1::Scene::PrepareForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    assert( !makingDynamic );
    assert( !makingStatic );

    geomInfoMgr->PrepareForFrame( frameIndex );

    makingDynamic = asManager->BeginDynamicGeometry( cmd, frameIndex );
    dynamicUniqueIDs.clear();
}

void RTGL1::Scene::SubmitForFrame( VkCommandBuffer                         cmd,
                                   uint32_t                                frameIndex,
                                   const std::shared_ptr< GlobalUniform >& uniform,
                                   uint32_t uniformData_rayCullMaskWorld,
                                   bool     allowGeometryWithSkyFlag,
                                   bool     disableRTGeometry )
{
    // always submit dynamic geometry on the frame ending
    asManager->SubmitDynamicGeometry( makingDynamic, cmd, frameIndex );


    // copy geom and tri infos to device-local
    geomInfoMgr->CopyFromStaging( cmd, frameIndex );


    // prepare tlas infos, and fill uniform with info about that tlas
    const auto [ prepare, push ] = asManager->PrepareForBuildingTLAS( frameIndex,
                                                                      *uniform->GetData(),
                                                                      uniformData_rayCullMaskWorld,
                                                                      allowGeometryWithSkyFlag,
                                                                      disableRTGeometry );

    // upload uniform data
    uniform->Upload( cmd, frameIndex );


    vertPreproc->Preprocess(
        cmd, frameIndex, VERT_PREPROC_MODE_ONLY_DYNAMIC, *uniform, *asManager, push );


    asManager->BuildTLAS( cmd, frameIndex, prepare );
}

RTGL1::UploadResult RTGL1::Scene::UploadPrimitive( uint32_t                   frameIndex,
                                                   const RgMeshInfo&          mesh,
                                                   const RgMeshPrimitiveInfo& primitive,
                                                   const TextureManager&      textureManager,
                                                   bool                       isStatic )
{
    uint64_t uniqueID = UniqueID::MakeForPrimitive( mesh, primitive );

    if( !isStatic )
    {
        if( mesh.isExportable )
        {
            // if dynamic-exportable was already uploaded
            // (i.e. found a matching mesh inside a static scene)
            // otherwise, continue as dynamic
            if( StaticMeshExists( mesh ) )
            {
                return UploadResult::ExportableStatic;
            }
        }
    }

    if( !InsertPrimitiveInfo( uniqueID, isStatic, mesh, primitive ) )
    {
        return UploadResult::Fail;
    }

    if( !asManager->AddMeshPrimitive(
            frameIndex, mesh, primitive, uniqueID, isStatic, textureManager, *geomInfoMgr ) )
    {
        return UploadResult::Fail;
    }

    return isStatic
               ? ( mesh.isExportable ? UploadResult::ExportableStatic : UploadResult::Static )
               : ( mesh.isExportable ? UploadResult::ExportableDynamic : UploadResult::Dynamic );
}

RTGL1::UploadResult RTGL1::Scene::UploadLight( uint32_t               frameIndex,
                                               const GenericLightPtr& light,
                                               LightManager*          lightManager,
                                               bool                   isStatic )
{
    bool isExportable =
        std::visit( []( auto&& specific ) { return specific->isExportable; }, light );

    if( !isStatic )
    {
        if( isExportable )
        {
            if( StaticLightExists( light ) )
            {
                return UploadResult::ExportableStatic;
            }
        }
    }

    if( !InsertLightInfo( isStatic, light ) )
    {
        return UploadResult::Fail;
    }

    std::visit(
        [ & ]( auto&& specific ) {
            // adding static to light manager is done separately in SubmitStaticLights
            if( !isStatic )
            {
                assert( lightManager );
                lightManager->Add( frameIndex, *specific );
            }
        },
        light );

    return isStatic ? ( isExportable ? UploadResult::ExportableStatic : UploadResult::Static )
                    : ( isExportable ? UploadResult::ExportableDynamic : UploadResult::Dynamic );
}

void RTGL1::Scene::SubmitStaticLights( uint32_t frameIndex, LightManager& lightManager ) const
{
    for( const GenericLight& l : staticLights )
    {
        std::visit( [ &frameIndex, &lightManager ](
                        auto&& specific ) { lightManager.Add( frameIndex, specific ); },
                    l );
    }
}

bool RTGL1::Scene::InsertPrimitiveInfo( uint64_t                   uniqueID,
                                        bool                       isStatic,
                                        const RgMeshInfo&          mesh,
                                        const RgMeshPrimitiveInfo& primitive )
{
    if( isStatic )
    {
        assert( !Utils::IsCstrEmpty( mesh.pMeshName ) );
        if( !Utils::IsCstrEmpty( mesh.pMeshName ) )
        {
            staticMeshNames.emplace( mesh.pMeshName );
        }

        if( !dynamicUniqueIDs.contains( uniqueID ) )
        {
            auto [ iter, isNew ] = staticUniqueIDs.emplace( uniqueID );
            if( isNew )
            {
                return true;
            }
        }
    }
    else
    {
        if( !staticUniqueIDs.contains( uniqueID ) )
        {
            auto [ iter, isNew ] = dynamicUniqueIDs.emplace( uniqueID );
            if( isNew )
            {
                return true;
            }
        }
    }

    debug::Error( "Mesh primitive ({}->{}) with ID ({}->{}): "
                  "Trying to upload but a primitive with the same ID already exists",
                  Utils::SafeCstr( mesh.pMeshName ),
                  Utils::SafeCstr( primitive.pPrimitiveNameInMesh ),
                  mesh.uniqueObjectID,
                  primitive.primitiveIndexInMesh );
    return false;
}

bool RTGL1::Scene::InsertLightInfo( bool isStatic, const GenericLightPtr& light )
{
    constexpr auto getIdFromRef = []( const GenericLight& l ) -> uint64_t {
        return std::visit( []( auto&& specific ) { return specific.uniqueID; }, l );
    };
    constexpr auto getId = []( const GenericLightPtr& l ) -> uint64_t {
        return std::visit( []( auto&& specific ) { return specific->uniqueID; }, l );
    };

    if( isStatic )
    {
        // just check that there's no id collision
        auto foundSameId =
            std::ranges::find_if( staticLights, [ &light ]( const GenericLight& other ) {
                return getIdFromRef( other ) == getId( light );
            } );

        if( foundSameId != staticLights.end() )
        {
            debug::Error(
                "Trying add a static light with a uniqueID {} that other light already has",
                getId( light ) );
            return false;
        }

        // add to the list
        std::visit(
            [ this ]( auto&& specific ) { return this->staticLights.push_back( *specific ); },
            light );
        return true;
    }
    else
    {
        return true;
    }
}

void RTGL1::Scene::NewScene( VkCommandBuffer           cmd,
                             uint32_t                  frameIndex,
                             const GltfImporter&       staticScene,
                             TextureManager&           textureManager,
                             const TextureMetaManager& textureMeta )
{
    staticUniqueIDs.clear();
    staticMeshNames.clear();
    staticLights.clear();

    textureManager.FreeAllImportedMaterials( frameIndex );

    assert( !makingStatic );
    makingStatic = asManager->BeginStaticGeometry();

    if( staticScene )
    {
        staticScene.UploadToScene( cmd, frameIndex, *this, textureManager, textureMeta );
    }
    else
    {
        debug::Info( "New scene is empty" );
    }

    debug::Info( "Rebuilding static geometry. Waiting device idle..." );
    asManager->SubmitStaticGeometry( makingStatic );

    debug::Info( "Static geometry was rebuilt" );
}

const std::shared_ptr< RTGL1::ASManager >& RTGL1::Scene::GetASManager()
{
    return asManager;
}

const std::shared_ptr< RTGL1::VertexPreprocessing >& RTGL1::Scene::GetVertexPreprocessing()
{
    return vertPreproc;
}

bool RTGL1::Scene::StaticMeshExists( const RgMeshInfo& mesh ) const
{
    if( Utils::IsCstrEmpty( mesh.pMeshName ) )
    {
        return false;
    }

    // TODO: actually, need to consider RgMeshInfo::uniqueObjectID,
    // as there might be different instances of the same mesh
    return std::ranges::contains( staticMeshNames, std::string( mesh.pMeshName ) );
}

bool RTGL1::Scene::StaticLightExists( const GenericLightPtr& light ) const
{
    std::visit( []( auto&& specific ) { assert( specific->isExportable ); }, light );

    // TODO: compare ID-s?
    return !staticLights.empty();
}


RTGL1::SceneImportExport::SceneImportExport( std::filesystem::path _scenesFolder,
                                             const RgFloat3D&      _worldUp,
                                             const RgFloat3D&      _worldForward,
                                             const float&          _worldScale )
    : scenesFolder( std::move( _scenesFolder ) )
    , worldUp( Utils::SafeNormalize( _worldUp, { 0, 1, 0 } ) )
    , worldForward( Utils::SafeNormalize( _worldForward, { 0, 0, 1 } ) )
    , worldScale( std::max( 0.0f, _worldScale ) )
{
}

void RTGL1::SceneImportExport::PrepareForFrame()
{
    if( exportRequested )
    {
        exporter        = std::make_unique< GltfExporter >( MakeWorldTransform() );
        exportRequested = false;
    }
}

void RTGL1::SceneImportExport::CheckForNewScene( std::string_view    mapName,
                                                 VkCommandBuffer     cmd,
                                                 uint32_t            frameIndex,
                                                 Scene&              scene,
                                                 TextureManager&     textureManager,
                                                 TextureMetaManager& textureMeta )
{
    if( currentMap != mapName || reimportRequested )
    {
        reimportRequested = false;
        debug::Verbose( "Starting new scene..." );

        currentMap = mapName;

        // before importer, as it relies on texture properties
        textureMeta.RereadFromFiles( GetImportMapName() );

        {
            auto staticScene =
                GltfImporter( MakeGltfPath( GetImportMapName() ), MakeWorldTransform() );

            scene.NewScene( cmd, frameIndex, staticScene, textureManager, textureMeta );
        }
        debug::Verbose( "New scene is ready" );
    }
}

void RTGL1::SceneImportExport::TryExport( TextureManager& textureManager )
{
    if( exporter )
    {
        exporter->ExportToFiles( MakeGltfPath( GetExportMapName() ), textureManager );
        exporter.reset();
    }
}

void RTGL1::SceneImportExport::RequestReimport()
{
    reimportRequested = true;
}

void RTGL1::SceneImportExport::OnFileChanged( FileType type, const std::filesystem::path& filepath )
{
    if( type == FileType::GLTF && filepath == MakeGltfPath( GetImportMapName() ) )
    {
        debug::Verbose( "Hot-reloading GLTF..." );
        RequestReimport();
    }
}

RTGL1::GltfExporter* RTGL1::SceneImportExport::TryGetExporter()
{
    return exporter.get();
}

const RgFloat3D& RTGL1::SceneImportExport::GetWorldUp() const
{
    if( dev.worldTransform.enable )
    {
        return dev.worldTransform.up;
    }

    assert( !Utils::IsAlmostZero( worldUp ) );
    return worldUp;
}

const RgFloat3D& RTGL1::SceneImportExport::GetWorldForward() const
{
    if( dev.worldTransform.enable )
    {
        return dev.worldTransform.forward;
    }

    assert( !Utils::IsAlmostZero( worldForward ) );
    return worldForward;
}

float RTGL1::SceneImportExport::GetWorldScale() const
{
    if( dev.worldTransform.enable )
    {
        return dev.worldTransform.scale;
    }

    assert( worldScale >= 0.0f );
    return worldScale;
}

RgTransform RTGL1::SceneImportExport::MakeWorldTransform() const
{
    return Utils::MakeTransform(
        Utils::Normalize( GetWorldUp() ), Utils::Normalize( GetWorldForward() ), GetWorldScale() );
}

std::string_view RTGL1::SceneImportExport::GetImportMapName() const
{
    if( dev.importName.enable )
    {
        dev.importName.value[ std::size( dev.importName.value ) - 1 ] = '\0';
        return dev.importName.value;
    }

    return currentMap;
}

std::string_view RTGL1::SceneImportExport::GetExportMapName() const
{
    if( dev.exportName.enable )
    {
        dev.exportName.value[ std::size( dev.exportName.value ) - 1 ] = '\0';
        return dev.exportName.value;
    }

    return currentMap;
}

std::filesystem::path RTGL1::SceneImportExport::MakeGltfPath( std::string_view mapName )
{
    auto exportName = std::string( mapName );

    std::ranges::replace( exportName, '\\', '_' );
    std::ranges::replace( exportName, '/', '_' );

    return scenesFolder / exportName / ( exportName + ".gltf" );
}

void RTGL1::SceneImportExport::RequestExport()
{
    exportRequested = true;
}
