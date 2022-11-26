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

namespace
{
constexpr uint32_t MAX_DEVICE_COUNT = 8;

rgl::unordered_map< RgInstance, std::unique_ptr< RTGL1::VulkanDevice > > G_DEVICES;

RgInstance GetNextID()
{
    return reinterpret_cast< RgInstance >( G_DEVICES.size() + 1024 );
}

RTGL1::VulkanDevice& GetDevice( RgInstance rgInstance )
{
    auto it = G_DEVICES.find( rgInstance );

    if( it == G_DEVICES.end() )
    {
        throw RTGL1::RgException( RG_RESULT_WRONG_INSTANCE );
    }

    return *( it->second );
}

void TryPrintError( RgInstance rgInstance, const char* pMessage, RgMessageSeverityFlags severity )
{
    auto it = G_DEVICES.find( rgInstance );

    if( it != G_DEVICES.end() )
    {
        it->second->Print( pMessage, severity );
    }
}
}



RgResult rgCreateInstance( const RgInstanceCreateInfo* pInfo, RgInstance* pResult )
{
    *pResult = nullptr;

    if( G_DEVICES.size() >= MAX_DEVICE_COUNT )
    {
        return RG_RESULT_WRONG_INSTANCE;
    }

    // insert new
    RgInstance rgInstance = GetNextID();
    assert( G_DEVICES.find( rgInstance ) == G_DEVICES.end() );

    try
    {
        G_DEVICES[ rgInstance ] = std::make_unique< RTGL1::VulkanDevice >( pInfo );
        *pResult                = rgInstance;
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
    return RG_RESULT_SUCCESS;
}

RgResult rgDestroyInstance( RgInstance rgInstance )
{
    if( G_DEVICES.find( rgInstance ) == G_DEVICES.end() )
    {
        return RG_RESULT_WRONG_INSTANCE;
    }

    try
    {
        G_DEVICES.erase( rgInstance );
    }
    catch( RTGL1::RgException& e )
    {
        TryPrintError( rgInstance, e.what(), RG_MESSAGE_SEVERITY_ERROR );
        return e.GetErrorCode();
    }
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
        TryPrintError( rgInstance, e.what(), RG_MESSAGE_SEVERITY_ERROR );
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
        TryPrintError( rgInstance, e.what(), RG_MESSAGE_SEVERITY_ERROR );
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

void rgUtilImScratchBegin( RgInstance instance )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchBegin );
}

void rgUtilImScratchVertex( RgInstance instance, float x, float y, float z )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchVertex, x, y, z );
}

void rgUtilImScratchTexCoord( RgInstance instance, float u, float v )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchTexCoord, u, v );
}

void rgUtilImScratchColor( RgInstance instance, uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchColor, r, g, b, a );
}

void rgUtilImScratchEnd( RgInstance instance )
{
    Call( instance, &RTGL1::VulkanDevice::ImScratchEnd );
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
    return
        ( uint32_t( a ) << 24 ) | 
        ( uint32_t( b ) << 16 ) | 
        ( uint32_t( g ) << 8 ) | 
        ( uint32_t( r ) );
}

RgColor4DPacked32 rgUtilPackColorFloat4D( float r, float g, float b, float a )
{
    auto toUint8 = []( float c ) {
        return uint8_t( std::clamp( int32_t( c * 255.0f ), 0, 255 ) );
    };

    return rgUtilPackColorByte4D( toUint8( r ), toUint8( g ), toUint8( b ), toUint8( a ) );
}

void rgUtilExportAsPNG( RgInstance instance, const void* pPixels, uint32_t width, uint32_t height, const char* pPath )
{
    Call( instance, &RTGL1::VulkanDevice::ExportAsPNG, pPixels, width, height, pPath );
}
