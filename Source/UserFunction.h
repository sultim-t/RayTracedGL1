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

#include "RTGL1/RTGL1.h"

namespace RTGL1
{

class UserPrint
{
public:
    UserPrint( PFN_rgPrint printFunc, void* pUserData );
    ~UserPrint() = default;

    UserPrint( const UserPrint& other )     = delete;
    UserPrint( UserPrint&& other ) noexcept = delete;
    UserPrint& operator=( const UserPrint& other ) = delete;
    UserPrint& operator=( UserPrint&& other ) noexcept = delete;

    void       Print( const char* pMessage, RgMessageSeverityFlags severity ) const;

private:
    PFN_rgPrint printFunc;
    void*       pUserData;
};



// A class to simplify calling rgOpenFile and rgCloseFile.
class UserFileLoad
{
    // This struct will automatically call rgCloseFile.
    // Must be contructed only by UserFileLoad::OpenFile.
    struct UserFileLoadHandle
    {
        ~UserFileLoadHandle();

        UserFileLoadHandle( UserFileLoadHandle&& other ) noexcept;
        UserFileLoadHandle& operator=( UserFileLoadHandle&& other ) noexcept;
        // restrict copying
        UserFileLoadHandle( const UserFileLoadHandle& other ) = delete;
        UserFileLoadHandle& operator=( const UserFileLoadHandle& other ) = delete;

                            operator bool() const;
        bool                Contains() const;

        const void*         pData;
        uint32_t            dataSize;

    private:
        friend class UserFileLoad;
        UserFileLoadHandle( const UserFileLoad* pUFL, const char* pFilePath );

    private:
        const UserFileLoad* pUFL;
        void*               pFileHandle;
    };

public:
    UserFileLoad( PFN_rgOpenFile openFileFunc, PFN_rgCloseFile closeFileFunc, void* pUserData );
    ~UserFileLoad() = default;

    UserFileLoad( const UserFileLoad& other )     = delete;
    UserFileLoad( UserFileLoad&& other ) noexcept = delete;
    UserFileLoad&      operator=( const UserFileLoad& other ) = delete;
    UserFileLoad&      operator=( UserFileLoad&& other ) noexcept = delete;

    bool               Exists() const;
    UserFileLoadHandle Open( const char* pFilePath ) const;

    // These function should be called only by UserFileLoadHandle's constructor/destructor.
    void               OpenFile( const char*  pFilePath,
                                 const void** ppOutData,
                                 uint32_t*    pOutDataSize,
                                 void**       ppOutFileUserHandle ) const;
    void               CloseFile( void* pFileUserHandle ) const;

private:
    PFN_rgOpenFile  openFileFunc;
    PFN_rgCloseFile closeFileFunc;
    void*           pUserData;
};

}
