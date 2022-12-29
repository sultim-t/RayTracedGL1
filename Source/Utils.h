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

#include <array>
#include <optional>

#include "Common.h"
#include "RTGL1/RTGL1.h"

#define RG_SET_VEC3( dst, x, y, z ) \
    ( dst )[ 0 ] = ( x );           \
    ( dst )[ 1 ] = ( y );           \
    ( dst )[ 2 ] = ( z )

#define RG_SET_VEC3_A( dst, xyz ) \
    ( dst )[ 0 ] = ( xyz )[ 0 ];  \
    ( dst )[ 1 ] = ( xyz )[ 1 ];  \
    ( dst )[ 2 ] = ( xyz )[ 2 ]

#define RG_ACCESS_VEC3( src ) ( src )[ 0 ], ( src )[ 1 ], ( src )[ 2 ]
#define RG_ACCESS_VEC4( src ) ( src )[ 0 ], ( src )[ 1 ], ( src )[ 2 ], ( src )[ 3 ]

#define RG_MAX_VEC3( dst, m )                       \
    ( dst )[ 0 ] = std::max( ( dst )[ 0 ], ( m ) ); \
    ( dst )[ 1 ] = std::max( ( dst )[ 1 ], ( m ) ); \
    ( dst )[ 2 ] = std::max( ( dst )[ 2 ], ( m ) )

#define RG_SET_VEC4( dst, x, y, z, w ) \
    ( dst )[ 0 ] = ( x );              \
    ( dst )[ 1 ] = ( y );              \
    ( dst )[ 2 ] = ( z );              \
    ( dst )[ 3 ] = ( w )

// clang-format off

#define RG_MATRIX_TRANSPOSED( /* RgTransform */ m )                                   \
    {                                                                                 \
        ( m ).matrix[ 0 ][ 0 ], ( m ).matrix[ 1 ][ 0 ], ( m ).matrix[ 2 ][ 0 ], 0.0f, \
        ( m ).matrix[ 0 ][ 1 ], ( m ).matrix[ 1 ][ 1 ], ( m ).matrix[ 2 ][ 1 ], 0.0f, \
        ( m ).matrix[ 0 ][ 2 ], ( m ).matrix[ 1 ][ 2 ], ( m ).matrix[ 2 ][ 2 ], 0.0f, \
        ( m ).matrix[ 0 ][ 3 ], ( m ).matrix[ 1 ][ 3 ], ( m ).matrix[ 2 ][ 3 ], 1.0f, \
    }

#define RG_TRANSFORM_IDENTITY   \
    {                           \
        1.0f, 0.0f, 0.0f, 0.0f, \
        0.0f, 1.0f, 0.0f, 0.0f, \
        0.0f, 0.0f, 1.0f, 0.0f, \
    }

// clang-format on

namespace RTGL1
{
enum NullifyTokenType
{
};
inline constexpr NullifyTokenType NullifyToken = {};

template< size_t Size >
struct FloatStorage
{
    FloatStorage()  = default;
    ~FloatStorage() = default;

    explicit FloatStorage( NullifyTokenType ) { memset( data, 0, sizeof( data ) ); }
    explicit FloatStorage( const float* ptr ) { memcpy( data, ptr, sizeof( data ) ); }

    FloatStorage( const FloatStorage& other )                = default;
    FloatStorage( FloatStorage&& other ) noexcept            = default;
    FloatStorage& operator=( const FloatStorage& other )     = default;
    FloatStorage& operator=( FloatStorage&& other ) noexcept = default;

    [[nodiscard]] const float* Get() const { return data; }
    float*                     Get() { return data; }

    /*const float& operator[]( size_t i ) const
    {
        assert( i < std::size( data ) );
        return data[ i ];
    }*/

    float data[ Size ];
};

using Float16D = FloatStorage< 16 >;
using Float4D  = FloatStorage< 4 >;

// Because std::optional requires explicit constructor
#define IfNotNull( ptr, ifnotnull ) \
    ( ( ptr ) != nullptr ? std::optional( ( ifnotnull ) ) : std::nullopt )

namespace Utils
{
    void BarrierImage( VkCommandBuffer                cmd,
                       VkImage                        image,
                       VkAccessFlags                  srcAccessMask,
                       VkAccessFlags                  dstAccessMask,
                       VkImageLayout                  oldLayout,
                       VkImageLayout                  newLayout,
                       VkPipelineStageFlags           srcStageMask,
                       VkPipelineStageFlags           dstStageMask,
                       const VkImageSubresourceRange& subresourceRange );

    void BarrierImage( VkCommandBuffer                cmd,
                       VkImage                        image,
                       VkAccessFlags                  srcAccessMask,
                       VkAccessFlags                  dstAccessMask,
                       VkImageLayout                  oldLayout,
                       VkImageLayout                  newLayout,
                       const VkImageSubresourceRange& subresourceRange );

    void BarrierImage( VkCommandBuffer      cmd,
                       VkImage              image,
                       VkAccessFlags        srcAccessMask,
                       VkAccessFlags        dstAccessMask,
                       VkImageLayout        oldLayout,
                       VkImageLayout        newLayout,
                       VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask );

    void BarrierImage( VkCommandBuffer cmd,
                       VkImage         image,
                       VkAccessFlags   srcAccessMask,
                       VkAccessFlags   dstAccessMask,
                       VkImageLayout   oldLayout,
                       VkImageLayout   newLayout );

    void ASBuildMemoryBarrier( VkCommandBuffer cmd );

    void WaitForFence( VkDevice device, VkFence fence );
    void ResetFence( VkDevice device, VkFence fence );
    void WaitAndResetFence( VkDevice device, VkFence fence );
    void WaitAndResetFences( VkDevice device, VkFence fence_A, VkFence fence_B );

    VkFormat ToUnorm( VkFormat f );
    VkFormat ToSRGB( VkFormat f );
    bool     IsSRGB( VkFormat f );

    template< typename T >
    T Align( const T& v, const T& alignment );
    template< typename T >
    bool IsPow2( const T& v );

    bool AreViewportsSame( const VkViewport& a, const VkViewport& b );

    bool        IsAlmostZero( const float v[ 3 ] );
    bool        IsAlmostZero( const RgFloat3D& v );
    bool        IsAlmostZero( const RgMatrix3D& m );
    float       Dot( const float a[ 3 ], const float b[ 3 ] );
    float       Length( const float v[ 3 ] );
    bool        TryNormalize( float inout[ 3 ] );
    void        Normalize( float inout[ 3 ] );
    RgFloat3D   Normalize( const RgFloat3D& v );
    RgFloat3D   SafeNormalize( const RgFloat3D& v, const RgFloat3D& fallback );
    void        Negate( float inout[ 3 ] );
    void        Nullify( float inout[ 3 ] );
    void        Cross( const float a[ 3 ], const float b[ 3 ], float r[ 3 ] );
    RgFloat3D   GetUnnormalizedNormal( const RgFloat3D positions[ 3 ] );
    bool        GetNormalAndArea( const RgFloat3D positions[ 3 ], RgFloat3D& normal, float& area );
    // In terms of GLSL: mat3(a), where a is mat4.
    // The remaining values are initialized with identity matrix.
    void        SetMatrix3ToGLSLMat4( float dst[ 16 ], const RgMatrix3D& src );
    RgTransform MakeTransform( const RgFloat3D& up, const RgFloat3D& forward, float scale );
    RgTransform MakeTransform( const RgFloat3D& position, const RgFloat3D& forward );

    uint32_t        GetPreviousByModulo( uint32_t value, uint32_t count );
    inline uint32_t PrevFrame( uint32_t frameIndex )
    {
        return GetPreviousByModulo( frameIndex, MAX_FRAMES_IN_FLIGHT );
    }

    uint32_t GetWorkGroupCount( float size, uint32_t groupSize );
    uint32_t GetWorkGroupCount( uint32_t size, uint32_t groupSize );

    template< typename T1, typename T2 >
        requires( std::is_integral_v< T1 > && std::is_integral_v< T2 > )
    uint32_t GetWorkGroupCountT( T1 size, T2 groupSize );

    template< typename ReturnType = RgFloat4D >
        requires( std::is_same_v< ReturnType, RgFloat4D > ||
                  std::is_same_v< ReturnType, RgFloat3D > )
    constexpr ReturnType UnpackColor4DPacked32( RgColor4DPacked32 c )
    {
        if constexpr( std::is_same_v< ReturnType, RgFloat3D > )
        {
            return RgFloat3D{ {
                float( ( c >> 0 ) & 255u ) / 255.0f,
                float( ( c >> 8 ) & 255u ) / 255.0f,
                float( ( c >> 16 ) & 255u ) / 255.0f,
            } };
        }
        if constexpr( std::is_same_v< ReturnType, RgFloat4D > )
        {
            return RgFloat4D{ {
                float( ( c >> 0 ) & 255u ) / 255.0f,
                float( ( c >> 8 ) & 255u ) / 255.0f,
                float( ( c >> 16 ) & 255u ) / 255.0f,
                float( ( c >> 24 ) & 255u ) / 255.0f,
            } };
        }
        assert( 0 );
        return {};
    }

    template< bool WithAlpha >
    constexpr bool IsColor4DPacked32Zero( RgColor4DPacked32 c )
    {
        uint32_t mask = WithAlpha ? 0xFFFFFFFF : 0x00FFFFFF;
        return ( c & mask ) == 0;
    }

    constexpr std::array< uint8_t, 4 > UnpackColor4DPacked32Components( RgColor4DPacked32 c )
    {
        return { {
            uint8_t( ( c >> 0 ) & 255u ),
            uint8_t( ( c >> 8 ) & 255u ),
            uint8_t( ( c >> 16 ) & 255u ),
            uint8_t( ( c >> 24 ) & 255u ),
        } };
    }

    constexpr float UnpackAlphaFromPacked32( RgColor4DPacked32 c )
    {
        return float( ( c >> 24 ) & 255u ) / 255.0f;
    }

    inline bool IsCstrEmpty( const char* cstr )
    {
        return cstr == nullptr || *cstr == '\0';
    }
    inline const char* SafeCstr( const char* cstr )
    {
        return cstr ? cstr : "";
    }

    constexpr RgColor4DPacked32 PackColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
    {
        return ( uint32_t( a ) << 24 ) | ( uint32_t( b ) << 16 ) | ( uint32_t( g ) << 8 ) |
               ( uint32_t( r ) );
    }

    constexpr RgColor4DPacked32 PackColorFromFloat( float r, float g, float b, float a )
    {
        constexpr auto toUint8 = []( float c ) {
            return uint8_t( std::clamp( int32_t( c * 255.0f ), 0, 255 ) );
        };

        return PackColor( toUint8( r ), toUint8( g ), toUint8( b ), toUint8( a ) );
    }

    constexpr RgColor4DPacked32 PackColorFromFloat( const float ( &rgba )[ 4 ] )
    {
        return PackColorFromFloat( rgba[ 0 ], rgba[ 1 ], rgba[ 2 ], rgba[ 3 ] );
    }

    constexpr float Saturate( float v )
    {
        return std::clamp( v, 0.0f, 1.0f );
    }

// clang-format off
    // Column memory order!
    #define RG_TRANSFORM_TO_GLTF_MATRIX( t ) {                                      \
        ( t ).matrix[ 0 ][ 0 ], ( t ).matrix[ 1 ][ 0 ], ( t ).matrix[ 2 ][ 0 ], 0,  \
        ( t ).matrix[ 0 ][ 1 ], ( t ).matrix[ 1 ][ 1 ], ( t ).matrix[ 2 ][ 1 ], 0,  \
        ( t ).matrix[ 0 ][ 2 ], ( t ).matrix[ 1 ][ 2 ], ( t ).matrix[ 2 ][ 2 ], 0,  \
        ( t ).matrix[ 0 ][ 3 ], ( t ).matrix[ 1 ][ 3 ], ( t ).matrix[ 2 ][ 3 ], 1   }
    // clang-format on
};

template< typename T >
constexpr T clamp( const T& v, const T& v_min, const T& v_max )
{
    assert( v_min <= v_max );
    return std::min( v_max, std::max( v_min, v ) );
}

template< typename T >
bool Utils::IsPow2( const T& v )
{
    static_assert( std::is_integral_v< T > );
    return ( v != 0 ) && ( ( v & ( v - 1 ) ) == 0 );
}

template< typename T >
T Utils::Align( const T& v, const T& alignment )
{
    static_assert( std::is_integral_v< T > );
    assert( IsPow2( alignment ) );

    return ( v + alignment - 1 ) & ~( alignment - 1 );
}

template< typename T1, typename T2 >
    requires( std::is_integral_v< T1 > && std::is_integral_v< T2 > )
uint32_t Utils::GetWorkGroupCountT( T1 size, T2 groupSize )
{
    assert( size <= std::numeric_limits< uint32_t >::max() );
    assert( groupSize <= std::numeric_limits< uint32_t >::max() );

    return GetWorkGroupCount( static_cast< uint32_t >( size ),
                              static_cast< uint32_t >( groupSize ) );
}

}