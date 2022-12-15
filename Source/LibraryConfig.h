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

#pragma once

#include "Utils.h"

#include <filesystem>
#include <fstream>

namespace RTGL1::LibraryConfig
{
struct Config
{
    bool vulkanValidation = false;
    bool developerMode    = false;
    bool dlssValidation   = false;
    bool fpsMonitor       = false;
};

namespace detail
{
    inline void ProcessEntry( Config& dst, std::string_view entry )
    {
        if( entry == "vulkanvalidation" )
        {
            dst.vulkanValidation = true;
        }
        else if( entry == "developer" )
        {
            dst.developerMode = true;
        }
        else if( entry == "dlssvalidation" )
        {
            dst.dlssValidation = true;
        }
        else if( entry == "fpsmonitor" )
        {
            dst.fpsMonitor = true;
        }
    }
}

inline Config Read( const char* pPath )
{
    if( Utils::IsCstrEmpty( pPath ) )
    {
        pPath = "RayTracedGL1.txt";
    }

    auto path = std::filesystem::path( pPath );

    if( std::filesystem::exists( path ) )
    {
        std::ifstream file( path );

        if( file.is_open() )
        {
            Config result = {};

            for( std::string line; std::getline( file, line ); )
            {
                std::ranges::transform( line, line.begin(), ::tolower );

                detail::ProcessEntry( result, line );
            }

            return result;
        }
    }

    return {};
}
}
