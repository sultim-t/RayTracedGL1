// macOS port: Apple MetalFX temporal upscaler in place of AMD FSR2.
//
// RTGL1 already produces everything FSR2 needs (tonemapped colour, NDC depth,
// motion vectors, camera jitter). On macOS the FSR2 slot is filled by
// MTLFXTemporalScaler instead. VulkanDevice submits the Vulkan command buffer
// right before calling Apply(); this file then encodes its own Metal command
// buffer and the Vulkan frame continues in a fresh command buffer. MoltenVK may
// commit submissions from a background thread, so Apply() waits for the queue
// to go idle before, and for MetalFX to finish after.

#include "FSR2.h"

#if defined(__APPLE__) && !defined(RG_USE_AMD_FSR2)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <cstdio>
#include <cstdlib>

#include "Common.h"
#include "Framebuffers.h"
#include "HaltonSequence.h"
#include "RenderResolutionHelper.h"

struct FfxFsr2Context {};

namespace
{
struct MfxState
{
    VkDevice                    vkDevice = VK_NULL_HANDLE;
    PFN_vkExportMetalObjectsEXT exportFn = nullptr;

    id< MTLDevice >               device    = nil;
    id< MTLCommandQueue >         queue     = nil;
    id< MTLRenderPipelineState >  prepPipe  = nil;
    id< MTLDepthStencilState >    depthAlways = nil;
    id< MTLComputePipelineState > copyPipe  = nil;

    id< MTLFXTemporalScaler > scaler   = nil;
    id< MTLTexture >          colorTex = nil;
    id< MTLTexture >          depthTex = nil;
    id< MTLTexture >          motionTex = nil;
    id< MTLTexture >          outTex   = nil;

    uint32_t renderW = 0, renderH = 0, outW = 0, outH = 0;
    bool     reset          = true;
    bool     initTried      = false;
    bool     initOk         = false;
    bool     scalerFailed   = false;
    uint32_t loggedFailures = 0;
};

MfxState g;

void Log( const char* msg )
{
    fprintf( stderr, "[MetalFX] %s\n", msg );
}

const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VOut { float4 pos [[position]]; };

vertex VOut mfx_fullscreen_vs( uint vid [[vertex_id]] )
{
    float2 p = float2( float( ( vid << 1 ) & 2 ), float( vid & 2 ) );
    VOut   o;
    o.pos = float4( p * 2.0 - 1.0, 0.0, 1.0 );
    return o;
}

struct PrepOut
{
    float4 color  [[color(0)]];
    float2 motion [[color(1)]];
    float  depth  [[depth(any)]];
};

struct MfxParams
{
    float  tonemap;  // 1 = reversible tonemap around MetalFX
    float  maxInv;   // cap for the inverse tonemap
};

// colour (RG11B10) -> RGBA16F (sanitized, optionally tonemapped c/(1+max(c))),
// motion -> sanitized RG16F, NDC depth (R32F) -> Depth32Float
fragment PrepOut mfx_prep_fs( VOut in [[stage_in]],
                              texture2d< float > color  [[texture(0)]],
                              texture2d< float > depth  [[texture(1)]],
                              texture2d< float > motion [[texture(2)]],
                              constant MfxParams& prm   [[buffer(0)]] )
{
    uint2   p = uint2( in.pos.xy );
    PrepOut o;

    float3 c = color.read( p ).rgb;
    c        = select( c, float3( 0.0 ), !isfinite( c ) );
    c        = clamp( c, 0.0f, 65000.0f );
    if( prm.tonemap > 0.5 )
    {
        c = c / ( 1.0 + max( c.r, max( c.g, c.b ) ) );
    }
    o.color = float4( c, 1.0 );

    float2 m = motion.read( p ).rg;
    m        = select( m, float2( 0.0 ), !isfinite( m ) );
    o.motion = clamp( m, -2.0f, 2.0f );

    float d = depth.read( p ).r;
    o.depth = isfinite( d ) ? clamp( d, 0.0f, 1.0f ) : 1.0f;
    return o;
}

kernel void mfx_copy_out( texture2d< float, access::read >  src [[texture(0)]],
                          texture2d< float, access::write > dst [[texture(1)]],
                          constant MfxParams& prm               [[buffer(0)]],
                          uint2 gid [[thread_position_in_grid]] )
{
    if( gid.x >= dst.get_width() || gid.y >= dst.get_height() )
        return;
    float3 c = src.read( gid ).rgb;
    c        = select( c, float3( 0.0 ), !isfinite( c ) );
    c        = max( c, 0.0f );
    if( prm.tonemap > 0.5 )
    {
        float m = min( max( c.r, max( c.g, c.b ) ), prm.maxInv );
        c       = c / max( 1.0 - m, 1.0 - prm.maxInv );
    }
    dst.write( float4( c, 1.0 ), gid );
}
)";

bool InitDevice( VkDevice vkDevice, VkQueue vkQueue )
{
    if( g.initTried )
    {
        return g.initOk;
    }
    g.initTried = true;
    g.vkDevice  = vkDevice;

    g.exportFn = reinterpret_cast< PFN_vkExportMetalObjectsEXT >(
        vkGetDeviceProcAddr( vkDevice, "vkExportMetalObjectsEXT" ) );
    if( !g.exportFn )
    {
        Log( "vkExportMetalObjectsEXT is not available (VK_EXT_metal_objects not enabled?)" );
        return false;
    }

    if( vkQueue == VK_NULL_HANDLE )
    {
        Log( "no Vulkan queue was given to the upscaler" );
        return false;
    }

    VkExportMetalCommandQueueInfoEXT qInfo = { VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT };
    qInfo.queue                            = vkQueue;
    VkExportMetalDeviceInfoEXT dInfo       = { VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT };
    dInfo.pNext                            = &qInfo;
    VkExportMetalObjectsInfoEXT info       = { VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT };
    info.pNext                             = &dInfo;
    g.exportFn( vkDevice, &info );

    g.device = dInfo.mtlDevice;
    g.queue  = qInfo.mtlCommandQueue;
    if( !g.device || !g.queue )
    {
        Log( "couldn't get the Metal device / command queue from MoltenVK" );
        return false;
    }
    if( ![MTLFXTemporalScalerDescriptor supportsDevice:g.device] )
    {
        Log( "this GPU doesn't support the MetalFX temporal scaler" );
        return false;
    }

    NSError*        err = nil;
    id< MTLLibrary > lib = [g.device newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource]
                                                  options:nil
                                                    error:&err];
    if( !lib )
    {
        fprintf( stderr, "[MetalFX] shader compile failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
        return false;
    }

    MTLRenderPipelineDescriptor* rpd    = [MTLRenderPipelineDescriptor new];
    rpd.vertexFunction                  = [lib newFunctionWithName:@"mfx_fullscreen_vs"];
    rpd.fragmentFunction                = [lib newFunctionWithName:@"mfx_prep_fs"];
    rpd.colorAttachments[ 0 ].pixelFormat = MTLPixelFormatRGBA16Float;
    rpd.colorAttachments[ 1 ].pixelFormat = MTLPixelFormatRG16Float;
    rpd.depthAttachmentPixelFormat      = MTLPixelFormatDepth32Float;
    g.prepPipe = [g.device newRenderPipelineStateWithDescriptor:rpd error:&err];

    MTLDepthStencilDescriptor* dsd = [MTLDepthStencilDescriptor new];
    dsd.depthCompareFunction       = MTLCompareFunctionAlways;
    dsd.depthWriteEnabled          = YES;
    g.depthAlways = [g.device newDepthStencilStateWithDescriptor:dsd];

    g.copyPipe = [g.device newComputePipelineStateWithFunction:[lib newFunctionWithName:@"mfx_copy_out"]
                                                         error:&err];
    if( !g.prepPipe || !g.copyPipe || !g.depthAlways )
    {
        fprintf( stderr, "[MetalFX] pipeline creation failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
        return false;
    }

    Log( "MetalFX temporal upscaler available" );
    g.initOk = true;
    return true;
}

id< MTLTexture > TextureOf( VkImage image )
{
    VkExportMetalTextureInfoEXT t = { VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT };
    t.image                       = image;
    t.plane                       = VK_IMAGE_ASPECT_PLANE_0_BIT;
    VkExportMetalObjectsInfoEXT info = { VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT };
    info.pNext                       = &t;
    g.exportFn( g.vkDevice, &info );
    return t.mtlTexture;
}

void ReleaseScaler()
{
    g.scaler   = nil;
    g.colorTex = nil;
    g.depthTex = nil;
    g.motionTex = nil;
    g.outTex   = nil;
    g.renderW = g.renderH = g.outW = g.outH = 0;
}

bool EnsureScaler( uint32_t rw, uint32_t rh, uint32_t ow, uint32_t oh )
{
    if( g.scaler && g.renderW == rw && g.renderH == rh && g.outW == ow && g.outH == oh )
    {
        return true;
    }
    ReleaseScaler();

    MTLFXTemporalScalerDescriptor* d = [MTLFXTemporalScalerDescriptor new];
    d.colorTextureFormat             = MTLPixelFormatRGBA16Float;
    d.depthTextureFormat             = MTLPixelFormatDepth32Float;
    d.motionTextureFormat            = MTLPixelFormatRG16Float;
    d.outputTextureFormat            = MTLPixelFormatRGBA16Float;
    d.inputWidth                     = rw;
    d.inputHeight                    = rh;
    d.outputWidth                    = ow;
    d.outputHeight                   = oh;
    d.autoExposureEnabled            = NO; // input is already tonemapped (LDR)

    g.scaler = [d newTemporalScalerWithDevice:g.device];
    if( !g.scaler )
    {
        fprintf( stderr, "[MetalFX] can't create a %ux%u -> %ux%u temporal scaler (scale ratio unsupported?)\n", rw, rh, ow, oh );
        return false;
    }

    auto makeTex = [ & ]( MTLPixelFormat fmt, uint32_t w, uint32_t h, MTLTextureUsage usage ) {
        MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                                       width:w
                                                                                      height:h
                                                                                   mipmapped:NO];
        td.storageMode = MTLStorageModePrivate;
        td.usage       = usage;
        return [g.device newTextureWithDescriptor:td];
    };
    g.colorTex = makeTex( MTLPixelFormatRGBA16Float, rw, rh,
                          g.scaler.colorTextureUsage | MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead );
    g.depthTex = makeTex( MTLPixelFormatDepth32Float, rw, rh,
                          g.scaler.depthTextureUsage | MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead );
    g.motionTex = makeTex( MTLPixelFormatRG16Float, rw, rh,
                           g.scaler.motionTextureUsage | MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead );
    g.outTex   = makeTex( MTLPixelFormatRGBA16Float, ow, oh,
                          g.scaler.outputTextureUsage | MTLTextureUsageShaderRead );
    if( !g.colorTex || !g.depthTex || !g.motionTex || !g.outTex )
    {
        Log( "texture allocation failed" );
        ReleaseScaler();
        return false;
    }

    g.renderW = rw;
    g.renderH = rh;
    g.outW    = ow;
    g.outH    = oh;
    g.reset    = true;
    fprintf( stderr, "[MetalFX] temporal scaler %ux%u -> %ux%u\n", rw, rh, ow, oh );
    return true;
}
}


RTGL1::FSR2::FSR2( VkDevice _device, VkPhysicalDevice _physDevice )
    : device( _device ), physDevice( _physDevice ), context( std::make_unique< std::optional< FfxFsr2Context > >() )
{
}

RTGL1::FSR2::~FSR2()
{
    @autoreleasepool
    {
        ReleaseScaler();
        g.prepPipe    = nil;
        g.copyPipe    = nil;
        g.depthAlways = nil;
        g.queue       = nil;
        g.device      = nil;
        g.initTried   = false;
        g.initOk      = false;
    }
}

void RTGL1::FSR2::OnFramebuffersSizeChange( const ResolutionState& resolutionState )
{
    @autoreleasepool
    {
        // recreated lazily with the new sizes on the next Apply()
        ReleaseScaler();
    }
}

RTGL1::FramebufferImageIndex RTGL1::FSR2::Apply( VkCommandBuffer                        cmd,
                                                 uint32_t                               frameIndex,
                                                 const std::shared_ptr< Framebuffers >& framebuffers,
                                                 const RenderResolutionHelper&          renderResolution,
                                                 RgFloat2D                              jitterOffset,
                                                 float                                  timeDelta,
                                                 float                                  nearPlane,
                                                 float                                  farPlane,
                                                 float                                  fovVerticalRad )
{
    using FI = RTGL1::FramebufferImageIndex;

    @autoreleasepool
    {
        if( !InitDevice( device, queue ) )
        {
            return FI::FB_IMAGE_INDEX_FINAL;
        }

        const ResolutionState& rs = renderResolution.GetResolutionState();
        if( !EnsureScaler( rs.renderWidth, rs.renderHeight, rs.upscaledWidth, rs.upscaledHeight ) )
        {
            return FI::FB_IMAGE_INDEX_FINAL;
        }

        id< MTLTexture > finalTex  = TextureOf( framebuffers->GetImage( FI::FB_IMAGE_INDEX_FINAL, frameIndex ) );
        id< MTLTexture > depthNdc  = TextureOf( framebuffers->GetImage( FI::FB_IMAGE_INDEX_DEPTH_NDC, frameIndex ) );
        id< MTLTexture > motion    = TextureOf( framebuffers->GetImage( FI::FB_IMAGE_INDEX_MOTION_DLSS, frameIndex ) );
        id< MTLTexture > outputVk  = TextureOf( framebuffers->GetImage( FI::FB_IMAGE_INDEX_UPSCALED_PONG, frameIndex ) );
        if( !finalTex || !depthNdc || !motion || !outputVk )
        {
            if( g.loggedFailures++ < 5 )
            {
                Log( "couldn't get Metal textures for the upscaler inputs" );
            }
            return FI::FB_IMAGE_INDEX_FINAL;
        }

        struct
        {
            float tonemap;
            float maxInv;
        } prm = { 1.0f, 0.998f };

        // MoltenVK commits its submissions from a background thread when ray tracing
        // is enabled, so MetalFX could otherwise run before the frame it upscales.
        // Wait for all Vulkan work first, and for MetalFX to finish afterwards.
        vkQueueWaitIdle( queue );

        id< MTLCommandBuffer > cb = [g.queue commandBuffer];
        cb.label                  = @"RTGL1 MetalFX";

        // 1. convert inputs into formats MetalFX accepts
        {
            MTLRenderPassDescriptor* rp            = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.colorAttachments[ 0 ].texture     = g.colorTex;
            rp.colorAttachments[ 0 ].loadAction  = MTLLoadActionDontCare;
            rp.colorAttachments[ 0 ].storeAction = MTLStoreActionStore;
            rp.colorAttachments[ 1 ].texture     = g.motionTex;
            rp.colorAttachments[ 1 ].loadAction  = MTLLoadActionDontCare;
            rp.colorAttachments[ 1 ].storeAction = MTLStoreActionStore;
            rp.depthAttachment.texture           = g.depthTex;
            rp.depthAttachment.loadAction        = MTLLoadActionDontCare;
            rp.depthAttachment.storeAction       = MTLStoreActionStore;

            id< MTLRenderCommandEncoder > enc = [cb renderCommandEncoderWithDescriptor:rp];
            enc.label                         = @"MetalFX input prep";
            [enc setRenderPipelineState:g.prepPipe];
            [enc setDepthStencilState:g.depthAlways];
            [enc setFragmentTexture:finalTex atIndex:0];
            [enc setFragmentTexture:depthNdc atIndex:1];
            [enc setFragmentTexture:motion atIndex:2];
            [enc setFragmentBytes:&prm length:sizeof( prm ) atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [enc endEncoding];
        }

        // 2. temporal upscale
        {
            // same convention as the FSR2 / DLSS paths: negated camera jitter
            g.scaler.colorTexture       = g.colorTex;
            g.scaler.depthTexture       = g.depthTex;
            g.scaler.motionTexture      = g.motionTex;
            g.scaler.outputTexture      = g.outTex;
            g.scaler.jitterOffsetX      = -jitterOffset.data[ 0 ];
            g.scaler.jitterOffsetY      = -jitterOffset.data[ 1 ];
            g.scaler.motionVectorScaleX = static_cast< float >( rs.renderWidth );
            g.scaler.motionVectorScaleY = static_cast< float >( rs.renderHeight );
            g.scaler.depthReversed      = NO;
            g.scaler.reset              = g.reset;
            [g.scaler encodeToCommandBuffer:cb];
            g.reset = false;
        }

        // 3. copy the result into RTGL1's upscaled framebuffer
        {
            id< MTLComputeCommandEncoder > enc = [cb computeCommandEncoder];
            enc.label                          = @"MetalFX output copy";
            [enc setComputePipelineState:g.copyPipe];
            [enc setTexture:g.outTex atIndex:0];
            [enc setTexture:outputVk atIndex:1];
            [enc setBytes:&prm length:sizeof( prm ) atIndex:0];
            [enc dispatchThreads:MTLSizeMake( rs.upscaledWidth, rs.upscaledHeight, 1 )
                threadsPerThreadgroup:MTLSizeMake( 8, 8, 1 )];
            [enc endEncoding];
        }

        [cb commit];
        [cb waitUntilCompleted];
    }

    return FI::FB_IMAGE_INDEX_UPSCALED_PONG;
}

RgFloat2D RTGL1::FSR2::GetJitter( const ResolutionState& resolutionState, uint32_t frameId )
{
    return HaltonSequence::GetJitter_Halton23( frameId );
}

bool RTGL1::FSR2::IsFsr2Available()
{
    static int available = -1;
    if( available < 0 )
    {
        @autoreleasepool
        {
            id< MTLDevice > dev = MTLCreateSystemDefaultDevice();
            available = ( dev && [MTLFXTemporalScalerDescriptor supportsDevice:dev] ) ? 1 : 0;
        }
    }
    return available == 1;
}

#endif // __APPLE__ && !RG_USE_AMD_FSR2
