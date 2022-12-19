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

bool RTGL1::Scene::Upload( uint32_t                   frameIndex,
                           const RgMeshInfo&          mesh,
                           const RgMeshPrimitiveInfo& primitive,
                           const TextureManager&      textureManager )
{
    uint64_t uniqueID = UniqueID::MakeForPrimitive( mesh, primitive );

    if( DoesUniqueIDExist( uniqueID ) )
    {
        debug::Error( "Uploading mesh primitive ({}->{}) with ID ({}->{}), but it already exists",
                      Utils::SafeCstr( mesh.pMeshName ),
                      Utils::SafeCstr( primitive.pPrimitiveNameInMesh ),
                      mesh.uniqueObjectID,
                      primitive.primitiveIndexInMesh );
        return false;
    }

    if( asManager->AddMeshPrimitive( frameIndex, mesh, primitive, false, textureManager ) )
    {
        dynamicUniqueIDs.emplace( uniqueID );
        return true;
    }

    return false;
}

void RTGL1::Scene::StartStatic()
{
    assert( !makingStatic );
    makingStatic = asManager->BeginStaticGeometry();

    staticUniqueIDs.clear();
}

void RTGL1::Scene::SubmitStatic( VkCommandBuffer cmd )
{
    asManager->SubmitStaticGeometry( makingStatic );
}

const std::shared_ptr< RTGL1::ASManager >& RTGL1::Scene::GetASManager()
{
    return asManager;
}

const std::shared_ptr< RTGL1::VertexPreprocessing >& RTGL1::Scene::GetVertexPreprocessing()
{
    return vertPreproc;
}

bool RTGL1::Scene::DoesUniqueIDExist( uint64_t uniqueID ) const
{
    return std::ranges::contains( dynamicUniqueIDs, uniqueID ) ||
           std::ranges::contains( staticUniqueIDs, uniqueID );
}
