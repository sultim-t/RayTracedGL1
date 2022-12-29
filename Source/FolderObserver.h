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

#include "Containers.h"
#include "IFileDependency.h"

#include <deque>
#include <filesystem>

namespace RTGL1
{

class FolderObserver
{
public:
    explicit FolderObserver( const std::filesystem::path& ovrdFolder );
    ~FolderObserver() = default;

    FolderObserver( const FolderObserver& other )                = delete;
    FolderObserver( FolderObserver&& other ) noexcept            = delete;
    FolderObserver& operator=( const FolderObserver& other )     = delete;
    FolderObserver& operator=( FolderObserver&& other ) noexcept = delete;

    void RecheckFiles();

    void Subscribe( const std::shared_ptr< IFileDependency >& subscriber )
    {
        subscribers.emplace_back( subscriber );
    }

public:
    using Clock = std::filesystem::file_time_type::clock;

    struct DependentFile
    {
        FileType              type;
        std::filesystem::path path;
        uint64_t              pathHash;
        Clock::time_point     lastWriteTime;

        friend std::strong_ordering operator<=>( const DependentFile& a,
                                                 const DependentFile& b ) noexcept
        {
            // only path
            return a.path <=> b.path;
        }
    };

private:
    std::vector< std::filesystem::path > foldersToCheck;

    Clock::time_point           lastCheck;
    std::deque< DependentFile > prevAllFiles;

    std::vector< std::weak_ptr< IFileDependency > > subscribers;

    template< typename Func, typename... Args >
    auto CallSubsbribers( Func f, Args&&... args )
    {
        for( auto& ws : subscribers )
        {
            if( auto s = ws.lock() )
            {
                ( ( *s ).*f )( std::forward< Args >( args )... );
            }
        }
    }
};

}
