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

#include "DebugPrint.h"

#include <glaze/glaze.hpp>
#include <glaze/api/impl.hpp>
#include <glaze/api/std/deque.hpp>
#include <glaze/api/std/unordered_set.hpp>
#include <glaze/api/std/span.hpp>
#include <glaze/file/file_ops.hpp>

#include <filesystem>
#include <fstream>
#include <optional>

// clang-format off
#define JSON_READER( Type )                     \
    template<>                                  \
    struct glz::meta< Type >                    \
    {                                           \
        using T                     = Type;     \
        static constexpr auto value = object(   

#define JSON_READER_END                     ); }
// clang-format on


namespace RTGL1::json_reader::detail
{
struct Version
{
    int version = -1;
};
}


// clang-format off
JSON_READER( RTGL1::json_reader::detail::Version )

    "version", &T::version

JSON_READER_END;
// clang-format on


namespace RTGL1::json_reader
{

namespace detail
{
    template< typename T, bool NoException = false >
    T ReadJson( const std::string& buffer )
    {
        constexpr auto options = glz::opts{
            .error_on_unknown_keys = false,
            .no_except             = NoException,
        };

        T value{};
        glz::read< options >( value, buffer );

        return value;
    };
}

template< typename T >
    requires( T::Version,
              std::is_same_v< decltype( T::Version ), const int >,
              T::RequiredVersion,
              std::is_same_v< decltype( T::RequiredVersion ), const int > )
std::optional< T > LoadFile( const std::filesystem::path& path )
{
    if( !std::filesystem::exists( path ) )
    {
        return std::nullopt;
    }

    std::stringstream buffer;
    {
        std::ifstream file( path );

        if( file.is_open() )
        {
            buffer << file.rdbuf();
        }
        else
        {
            return std::nullopt;
        }
    }

    try
    {
        auto [ version ] = detail::ReadJson< detail::Version, true >( buffer.str() );

        if( version < 0 )
        {
            debug::Warning(
                "Json parse fail on {}: Invalid version, or \"version\" field is not set",
                path.string() );
            return std::nullopt;
        }

        if( version < T::RequiredVersion )
        {
            debug::Warning( "Json data is too old {}: Minimum version is {}, but got {}",
                            path.string(),
                            T::RequiredVersion,
                            version );
            return std::nullopt;
        }

        return detail::ReadJson< T >( buffer.str() );
    }
    catch( std::exception& e )
    {
        debug::Warning( "Json parse fail on {}: {}", path.string(), e.what() );
        return std::nullopt;
    }
}

}