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

#include <optional>
#include <limits>

#include "Common.h"
#include "RTGL1/RTGL1.h"

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

        FloatStorage( const FloatStorage& other )     = default;
        FloatStorage( FloatStorage&& other ) noexcept = default;
        FloatStorage& operator=( const FloatStorage& other ) = default;
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
    using Float4D = FloatStorage< 4 >;

    // Because std::optional requires explicit constructor
    #define IfNotNull( ptr, ifnotnull ) \
        ( ( ptr ) != nullptr ? std::optional( ( ifnotnull ) ) : std::nullopt )

namespace Utils
{
    void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        const VkImageSubresourceRange &subresourceRange);

    void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        const VkImageSubresourceRange &subresourceRange);

    void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

    void BarrierImage(
        VkCommandBuffer cmd, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout, VkImageLayout newLayout);

    void ASBuildMemoryBarrier(
        VkCommandBuffer cmd
    );

    void WaitForFence(VkDevice device, VkFence fence);
    void ResetFence(VkDevice device, VkFence fence);
    void WaitAndResetFence(VkDevice device, VkFence fence);
    void WaitAndResetFences(VkDevice device, VkFence fence_A, VkFence fence_B);

    template<typename T> T    Align( const T& v, const T& alignment );
    template<typename T> bool IsPow2( const T& v );

    bool AreViewportsSame(const VkViewport &a, const VkViewport &b);

    bool IsAlmostZero( const float v[ 3 ] );
    bool IsAlmostZero(const RgFloat3D &v);
    bool IsAlmostZero(const RgMatrix3D &m);
    float Dot(const float a[3], const float b[3]);
    float Length(const float v[3]);
    void Normalize(float inout[3]);
    void Negate(float inout[3]);
    void Nullify(float inout[3]);
    void Cross(const float a[3], const float b[3], float r[3]);
    RgFloat3D GetUnnormalizedNormal(const RgFloat3D positions[3]);
    bool GetNormalAndArea(const RgFloat3D positions[3], RgFloat3D &normal, float &area);
    // In terms of GLSL: mat3(a), where a is mat4.
    // The remaining values are initialized with identity matrix.
    void SetMatrix3ToGLSLMat4(float dst[16], const RgMatrix3D &src);

    uint32_t GetPreviousByModulo(uint32_t value, uint32_t count);
    
    uint32_t GetWorkGroupCount(float size, uint32_t groupSize);
    uint32_t GetWorkGroupCount(uint32_t size, uint32_t groupSize);

    template< typename T1, typename T2 >
    requires( std::is_integral_v< T1 >&& std::is_integral_v< T2 > )
    uint32_t GetWorkGroupCountT( T1 size, T2 groupSize );
};

template<typename T>
constexpr T clamp(const T &v, const T &v_min, const T &v_max)
{
    assert(v_min <= v_max);
    return std::min(v_max, std::max(v_min, v));
}

template <typename T>
bool Utils::IsPow2(const T& v)
{
    static_assert(std::is_integral_v<T>);
    return (v != 0) && ((v & (v - 1)) == 0);
}

template <typename T>
T Utils::Align(const T& v, const T& alignment)
{
    static_assert(std::is_integral_v<T>);
    assert(IsPow2(alignment));

    return (v + alignment - 1) & ~(alignment - 1);
}

template< typename T1, typename T2 > requires( std::is_integral_v< T1 >&& std::is_integral_v< T2 > )
uint32_t Utils::GetWorkGroupCountT( T1 size, T2 groupSize )
{
    assert( size <= std::numeric_limits< uint32_t >::max() );
    assert( groupSize <= std::numeric_limits< uint32_t >::max() );

    return GetWorkGroupCount( static_cast< uint32_t >( size ),
                              static_cast< uint32_t >( groupSize ) );
}

}
