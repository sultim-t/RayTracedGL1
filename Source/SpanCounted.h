// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include <span>

namespace rgl
{

template< class T, size_t Extent = std::dynamic_extent >
class span_counted : public std::span< T, Extent >
{
public:
    using base      = std::span< T, Extent >;
    using size_type = typename base::size_type;

    using base::span;
    using base::operator=;

    constexpr explicit span_counted( const std::span< T, Extent >& other ) noexcept
        : base( other.data(), other.size() )
    {
    }

    void increment()
    {
        if( count < base::size() )
        {
            ++count;
        }
        else
        {
            assert( 0 );
        }
    }

    T& increment_and_get()
    {
        increment();
        return this->operator[]( count - 1 );
    }

    void reset_count() { count = 0; }

    size_type get_count() const { return count; }

    auto get_counted_subspan() const { return std::span( this->begin(), count ); }

private:
    size_type count{ 0 };
};


struct byte_subspan
{
    size_t offsetInBytes{ 0 };
    size_t sizeInBytes{ 0 };
};


struct index_subspan
{
    size_t elementsOffset{ 0 };
    size_t elementsCount{ 0 };
};


template< class T, size_t Extent = std::dynamic_extent >
class subspan_incremental : public std::span< T, Extent >
{
public:
    using base      = std::span< T, Extent >;
    using size_type = typename base::size_type;

    using base::operator=;

    constexpr subspan_incremental() noexcept : base() {}

    constexpr explicit subspan_incremental( const std::span< T, Extent >& other ) noexcept
        : base( other.data(), other.size() )
    {
    }

    void add_to_subspan( size_type index )
    {
        if( index < base::size() )
        {
            if( begin && end )
            {
                begin = std::min( *begin, index );
                end   = std::max( *end, index + 1 );
            }
            else
            {
                assert( !begin && !end );
                begin = index;
                end   = index + 1;
            }
        }
        else
        {
            assert( 0 );
        }
    }

    void reset_subspan()
    {
        begin = std::nullopt;
        end   = std::nullopt;
    }

    size_t count_in_subspan() const
    {
        if( begin && end )
        {
            return *end - *begin;
        }

        assert( !begin && !end );
        return 0;
    }

    index_subspan resolve_index_subspan( size_t elementsBaseOffset /* = 0 */ ) const
    {
        if( begin && end )
        {
            assert( count_in_subspan() > 0 );

            return index_subspan{
                .elementsOffset = elementsBaseOffset + *begin,
                .elementsCount  = count_in_subspan(),
            };
        }

        assert( !begin && !end );
        return index_subspan{};
    }

    byte_subspan resolve_byte_subspan( size_t baseOffsetInBytes /* = 0 */ ) const
    {
        assert( baseOffsetInBytes % sizeof( T ) == 0 );

        index_subspan i = resolve_index_subspan( baseOffsetInBytes / sizeof( T ) );

        return byte_subspan{
            .offsetInBytes = i.elementsOffset * sizeof( T ),
            .sizeInBytes   = i.elementsCount * sizeof( T ),
        };
    }

private:
    std::optional< size_type > begin{ std::nullopt };
    std::optional< size_type > end{ std::nullopt };
};

}
