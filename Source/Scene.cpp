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
#include "Generated/ShaderCommonC.h"
#include "RgException.h"
#include "UniqueID.h"

RTGL1::Scene::Scene( VkDevice                                _device,
                     const PhysicalDevice&                   _physDevice,
                     std::shared_ptr< MemoryAllocator >&     _allocator,
                     std::shared_ptr< CommandBufferManager > _cmdManager,
                     std::shared_ptr< TextureManager >       _textureManager,
                     const GlobalUniform&                    _uniform,
                     const ShaderManager&                    _shaderManager )
{
    VertexCollectorFilterTypeFlags_Init();

    geomInfoMgr  = std::make_shared< GeomInfoManager >( _device, _allocator );

    asManager = std::make_shared< ASManager >( _device,
                                               _physDevice,
                                               _allocator,
                                               std::move( _cmdManager ),
                                               std::move( _textureManager ),
                                               geomInfoMgr );

    vertPreproc =
        std::make_shared< VertexPreprocessing >( _device, _uniform, *asManager, _shaderManager );
}

void RTGL1::Scene::PrepareForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    dynamicUniqueIDToSimpleIndex.clear();

    geomInfoMgr->PrepareForFrame( frameIndex );

    // dynamic geomtry
    asManager->BeginDynamicGeometry( cmd, frameIndex );
}

void RTGL1::Scene::SubmitForFrame( VkCommandBuffer                         cmd,
                                   uint32_t                                frameIndex,
                                   const std::shared_ptr< GlobalUniform >& uniform,
                                   uint32_t uniformData_rayCullMaskWorld,
                                   bool     allowGeometryWithSkyFlag,
                                   bool     disableRTGeometry )
{
    // copy to device-local, if there were any tex coords change for static geometry
    asManager->ResubmitStaticTexCoords( cmd );

    // always submit dynamic geometry on the frame ending
    asManager->SubmitDynamicGeometry( cmd, frameIndex );


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
                           const RgMeshPrimitiveInfo& primitive )
{
    uint64_t uniqueID = UniqueID::MakeForPrimitive( mesh, primitive );
    assert( !DoesUniqueIDExist( uniqueID ) );
    
    if( auto simpleIndex = asManager->AddMeshPrimitive( frameIndex, mesh, primitive, false ) )
    {
        dynamicUniqueIDToSimpleIndex[ uniqueID ] = *simpleIndex;
        return true;
    }

    return false;
}

void RTGL1::Scene::StartNewScene( LightManager& lightManager )
{
    asManager->BeginStaticGeometry();
    asManager->SubmitStaticGeometry();

    lightManager.Reset();

    staticUniqueIDToSimpleIndex.clear();
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
    return staticUniqueIDToSimpleIndex.find( uniqueID ) != staticUniqueIDToSimpleIndex.end() ||
           dynamicUniqueIDToSimpleIndex.find( uniqueID ) != dynamicUniqueIDToSimpleIndex.end();
}

bool RTGL1::Scene::TryGetStaticSimpleIndex( uint64_t uniqueID, uint32_t* result ) const
{
    auto f = staticUniqueIDToSimpleIndex.find( uniqueID );

    if( f != staticUniqueIDToSimpleIndex.end() )
    {
        *result = f->second;
        return true;
    }

    return false;
}
