// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "LibraryConfig.h"

#include <glaze/glaze.hpp>
#include <glaze/api/impl.hpp>
#include <glaze/api/std/deque.hpp>
#include <glaze/api/std/unordered_set.hpp>
#include <glaze/api/std/span.hpp>


// clang-format off
template<>
struct glz::meta< RTGL1::LibraryConfig::Config >
{
    using T = RTGL1::LibraryConfig::Config;

    static constexpr auto value = object(
        "version",          &T::version,
        "developerMode",    &T::developerMode,
        "vulkanValidation", &T::vulkanValidation,
        "dlssValidation",   &T::dlssValidation,
        "fpsMonitor",       &T::fpsMonitor
    );
};
// clang-format on


RTGL1::LibraryConfig::Config RTGL1::LibraryConfig::Read( const char* pPath )
{
    if( Utils::IsCstrEmpty( pPath ) )
    {
        pPath = "RayTracedGL1.json";
    }

    auto path = std::filesystem::path( pPath );

    if( std::filesystem::exists( path ) )
    {
        std::stringstream buffer;
        {
            std::ifstream file( path );

            if( file.is_open() )
            {
                buffer << file.rdbuf();
            }
        }

        try
        {
            return glz::read_json< Config >( buffer.str() );
        }
        catch( std::exception& e )
        {
            debug::Warning( e.what() );
            return {};
        }
    }

    return {};
}
