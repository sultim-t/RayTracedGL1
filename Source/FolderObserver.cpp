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

#include "FolderObserver.h"

#include "Const.h"

namespace fs = std::filesystem;

namespace RTGL1
{
namespace
{
    constexpr auto CHECK_FREQUENCY = std::chrono::milliseconds( 500 );

    void InsertAllFolderFiles( std::deque< FolderObserver::DependentFile >& dst,
                               const fs::path&                              folder )
    {
        for( const fs::directory_entry& entry : fs::recursive_directory_iterator( folder ) )
        {
            FileType type = MakeFileType( entry.path() );

            if( type != FileType::Unknown )
            {
                dst.push_back( FolderObserver::DependentFile{
                    .type          = type,
                    .path          = entry.path(),
                    .pathHash      = std::hash< fs::path >{}( entry.path() ),
                    .lastWriteTime = entry.last_write_time(),
                } );
            }
        }
    }
}
}

RTGL1::FolderObserver::FolderObserver( const fs::path &ovrdFolder )
{
    foldersToCheck = {
        ovrdFolder / DATABASE_FOLDER,
        ovrdFolder / SCENES_FOLDER,
        ovrdFolder / SHADERS_FOLDER,
        ovrdFolder / TEXTURES_FOLDER,
        ovrdFolder / TEXTURES_FOLDER_DEV,
    };
}

void RTGL1::FolderObserver::RecheckFiles()
{
    if( Clock::now() - lastCheck < CHECK_FREQUENCY )
    {
        return;
    }


    std::deque< DependentFile > curAllFiles;
    for( const fs::path &f : foldersToCheck )
    {
        InsertAllFolderFiles( curAllFiles, f );
    }


    for( const auto& cur : curAllFiles )
    {
        bool foundInPrev = false;

        for( const auto& prev : prevAllFiles )
        {
            // if file previously existed
            if( cur.pathHash == prev.pathHash && cur.path == prev.path )
            {
                // if was changed
                if( cur.lastWriteTime != prev.lastWriteTime )
                {
                    CallSubsbribers( &IFileDependency::OnFileChanged, cur.type, cur.path );
                }

                foundInPrev = true;
                break;
            }
        }

        // if new file
        if( !foundInPrev )
        {
            CallSubsbribers( &IFileDependency::OnFileChanged, cur.type, cur.path );
        }
    }

    prevAllFiles = std::move( curAllFiles );
    lastCheck    = Clock::now();
}
