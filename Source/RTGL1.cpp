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

#include "VulkanDevice.h"
#include "RgException.h"

#include "TextureExporter.h"

namespace
{
#define INITIALIZED_RGINSTANCE ( reinterpret_cast< RgInstance >( 1024 ) )


RgInstance                             g_deviceRgInstance{ nullptr };
std::unique_ptr< RTGL1::VulkanDevice > g_device{};

RTGL1::VulkanDevice* TryGetDevice( RgInstance rgInstance )
{
    if( rgInstance == g_deviceRgInstance )
    {
        if( g_device )
        {
            return g_device.get();
        }
    }
    return nullptr;
}

RTGL1::VulkanDevice& GetDevice( RgInstance rgInstance )
{
    if( auto d = TryGetDevice( rgInstance ) )
    {
        return *d;
    }
    throw RTGL1::RgException( RG_RESULT_WRONG_INSTANCE );
}
}

namespace RTGL1::debug::detail
{
DebugPrintFn g_print{};
}



RgResult rgCreateInstance( const RgInstanceCreateInfo* pInfo, RgInstance* pResult )
{
    *pResult = nullptr;

    if( TryGetDevice( g_deviceRgInstance ) )
    {
        return RG_RESULT_ALREADY_INITIALIZED;
    }

    try
    {
        g_device = std::make_unique< RTGL1::VulkanDevice >( pInfo );
        g_deviceRgInstance = INITIALIZED_RGINSTANCE;

        *pResult = g_deviceRgInstance;
    }
    // TODO: VulkanDevice must clean all the resources if initialization failed!
    // So for now exceptions should not happen. But if they did, target application must be closed.
    catch( RTGL1::RgException& e )
    {
        // UserPrint class probably wasn't initialized, print manually
        if( pInfo->pfnPrint != nullptr )
        {
            pInfo->pfnPrint( e.what(), RG_MESSAGE_SEVERITY_ERROR, pInfo->pUserPrintData );
        }

        return e.GetErrorCode();
    }

    RTGL1::debug::detail::g_print = []( std::string_view msg, RgMessageSeverityFlags severity ) {
        if( g_device )
        {
            g_device->Print( msg, severity );
        }
    };

    return RG_RESULT_SUCCESS;
}

RgResult rgDestroyInstance( RgInstance rgInstance )
{
    if( !TryGetDevice( rgInstance ) )
    {
        return RG_RESULT_WRONG_INSTANCE;
    }

    try
    {
        g_device.reset();
        g_deviceRgInstance = nullptr;
    }
    catch( RTGL1::RgException& e )
    {
        RTGL1::debug::Error( e.what() );
        return e.GetErrorCode();
    }

    RTGL1::debug::detail::g_print = nullptr;

    return RG_RESULT_SUCCESS;
}

template< typename Func, typename... Args >
requires( 
            std::is_same_v< std::invoke_result_t< Func, RTGL1::VulkanDevice, Args... >, void > 
        )
static auto Call( RgInstance rgInstance, Func f, Args&&... args )
{
    try
    {
        RTGL1::VulkanDevice& dev = GetDevice( rgInstance );

        if( dev.IsSuspended() )
        {
            return RG_RESULT_SUCCESS;
        }

        ( dev.*f )( std::forward< Args >( args )... );
    }
    catch( RTGL1::RgException& e )
    {
        RTGL1::debug::Error( e.what() );
        return e.GetErrorCode();
    }
    return RG_RESULT_SUCCESS;
}

template< typename Func, typename... Args >
requires( 
            !std::is_same_v< std::invoke_result_t< Func, RTGL1::VulkanDevice, Args... >, void >
            && std::is_default_constructible_v< std::invoke_result_t< Func, RTGL1::VulkanDevice, Args... > > 
        )
static auto Call( RgInstance rgInstance, Func f, Args&&... args )
{
    using ReturnType = std::invoke_result_t< Func, RTGL1::VulkanDevice, Args... >;

    try
    {
        RTGL1::VulkanDevice& dev = GetDevice( rgInstance );

        if( !dev.IsSuspended() )
        {
            return ( dev.*f )( std::forward< Args >( args )... );
        }
    }
    catch( RTGL1::RgException& e )
    {
        RTGL1::debug::Error( e.what() );
    }
    return ReturnType{};
}



RgResult rgUploadMeshPrimitive( RgInstance instance, const RgMeshInfo* pMesh, const RgMeshPrimitiveInfo* pPrimitive )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadMeshPrimitive, pMesh, pPrimitive );
}

RgResult rgUploadNonWorldPrimitive( RgInstance instance, const RgMeshPrimitiveInfo* pPrimitive, const float* pViewProjection, const RgViewport* pViewport )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadNonWorldPrimitive, pPrimitive, pViewProjection, pViewport );
}

RgResult rgUploadDecal( RgInstance instance, const RgDecalUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadDecal, pInfo );
}

RgResult rgUploadLensFlare( RgInstance instance, const RgLensFlareUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadLensFlare, pInfo );
}

RgResult rgUploadDirectionalLight( RgInstance instance, const RgDirectionalLightUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadDirectionalLight, pInfo );
}

RgResult rgUploadSphericalLight( RgInstance instance, const RgSphericalLightUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadSphericalLight, pInfo );
}

RgResult rgUploadSpotLight( RgInstance instance, const RgSpotLightUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadSpotlight, pInfo );
}

RgResult rgUploadPolygonalLight( RgInstance instance, const RgPolygonalLightUploadInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::UploadPolygonalLight, pInfo );
}

RgResult rgProvideOriginalTexture( RgInstance instance, const RgOriginalTextureInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::ProvideOriginalTexture, pInfo );
}

RgResult rgProvideOriginalCubemapTexture( RgInstance instance, const RgOriginalCubemapInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::ProvideOriginalCubemapTexture, pInfo );
}

RgResult rgMarkOriginalTextureAsDeleted( RgInstance instance, const char* pTextureName )
{
    return Call( instance, &RTGL1::VulkanDevice::MarkOriginalTextureAsDeleted, pTextureName );
}

RgResult rgStartFrame( RgInstance instance, const RgStartFrameInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::StartFrame, pInfo );
}

RgResult rgDrawFrame( RgInstance instance, const RgDrawFrameInfo* pInfo )
{
    return Call( instance, &RTGL1::VulkanDevice::DrawFrame, pInfo );
}

RgPrimitiveVertex* rgUtilScratchAllocForVertices( RgInstance instance, uint32_t vertexCount )
{
    return Call( instance, &RTGL1::VulkanDevice::ScratchAllocForVertices, vertexCount );
}

void rgUtilScratchFree( RgInstance instance, const RgPrimitiveVertex* pPointer )
{
    Call( instance, &RTGL1::VulkanDevice::ScratchFree, pPointer );
}

void rgUtilScratchGetIndices( RgInstance              instance,
                              RgUtilImScratchTopology topology,
                              uint32_t                vertexCount,
                              const uint32_t**        ppOutIndices,
                              uint32_t*               pOutIndexCount )
{
    Call( instance,
          &RTGL1::VulkanDevice::ScratchGetIndices,
          topology,
          vertexCount,
          ppOutIndices,
          pOutIndexCount );
}

void rgUtilImScratchClear( RgInstance instance )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchClear );
}

void rgUtilImScratchStart( RgInstance instance, RgUtilImScratchTopology topology )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchStart, topology );
}

void rgUtilImScratchEnd( RgInstance instance )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchEnd );
}

void rgUtilImScratchVertex( RgInstance instance, float x, float y, float z )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchVertex, x, y, z );
}


void rgUtilImScratchNormal( RgInstance instance, float x, float y, float z )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchNormal, x, y, z );
}

void rgUtilImScratchTexCoord( RgInstance instance, float u, float v )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchTexCoord, u, v );
}

void rgUtilImScratchTexCoord_Layer1( RgInstance instance, float u, float v )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchTexCoord_Layer1, u, v );
}

void rgUtilImScratchTexCoord_Layer2( RgInstance instance, float u, float v )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchTexCoord_Layer2, u, v );
}

void rgUtilImScratchTexCoord_Layer3( RgInstance instance, float u, float v )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchTexCoord_Layer3, u, v );
}

void rgUtilImScratchColor( RgInstance instance, RgColor4DPacked32 color )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchColor, color );
}

void rgUtilImScratchSetToPrimitive( RgInstance instance, RgMeshPrimitiveInfo* pTarget )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchSetToPrimitive, pTarget );
}

RgBool32 rgUtilIsUpscaleTechniqueAvailable( RgInstance instance, RgRenderUpscaleTechnique technique )
{
    return Call( instance, &RTGL1::VulkanDevice::IsUpscaleTechniqueAvailable, technique );
}

const char* rgUtilGetResultDescription( RgResult result )
{
    return RTGL1::RgException::GetRgResultName( result );
}

RgColor4DPacked32 rgUtilPackColorByte4D( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    return RTGL1::Utils::PackColor( r, g, b, a );
}

RgColor4DPacked32 rgUtilPackColorFloat4D( float r, float g, float b, float a )
{
    return RTGL1::Utils::PackColorFromFloat( r, g, b, a );
}

void rgUtilExportAsTGA( RgInstance instance, const void* pPixels, uint32_t width, uint32_t height, const char* pPath )
{
    RTGL1::TextureExporter::WriteTGA( pPath, pPixels, { width, height } );
}
