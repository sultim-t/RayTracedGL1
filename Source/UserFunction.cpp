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

#include "UserFunction.h"

#include <cassert>

RTGL1::UserPrint::UserPrint( PFN_rgPrint _printFunc, void* _pUserData )
    : printFunc( _printFunc ), pUserData( _pUserData )
{
}

void RTGL1::UserPrint::Print( const char* pMessage, RgMessageSeverityFlags severity ) const
{
    if( printFunc != nullptr )
    {
        printFunc( pMessage, severity, pUserData );
    }
}



RTGL1::UserFileLoad::UserFileLoadHandle::UserFileLoadHandle( const UserFileLoad* _pUFL,
                                                             const char*         _pFilePath )
    : pData( nullptr ), dataSize( 0 ), pUFL( _pUFL ), pFileHandle( nullptr )
{
    assert( pUFL != nullptr );
    pUFL->OpenFile( _pFilePath, &pData, &dataSize, &pFileHandle );
}

RTGL1::UserFileLoad::UserFileLoadHandle::~UserFileLoadHandle()
{
    assert( pUFL != nullptr );
    pUFL->CloseFile( pFileHandle );
}

RTGL1::UserFileLoad::UserFileLoadHandle::UserFileLoadHandle( UserFileLoadHandle&& other ) noexcept
{
    this->pData       = other.pData;
    this->dataSize    = other.dataSize;
    this->pFileHandle = other.pFileHandle;
    this->pUFL        = other.pUFL;

    other.pData       = nullptr;
    other.dataSize    = 0;
    other.pFileHandle = nullptr;
    other.pUFL        = nullptr;
}

RTGL1::UserFileLoad::UserFileLoadHandle& RTGL1::UserFileLoad::UserFileLoadHandle::operator=(
    UserFileLoadHandle&& other ) noexcept
{
    this->pData       = other.pData;
    this->dataSize    = other.dataSize;
    this->pFileHandle = other.pFileHandle;
    this->pUFL        = other.pUFL;

    other.pData       = nullptr;
    other.dataSize    = 0;
    other.pFileHandle = nullptr;
    other.pUFL        = nullptr;

    return *this;
}

RTGL1::UserFileLoad::UserFileLoadHandle::operator bool() const
{
    return Contains();
}

bool RTGL1::UserFileLoad::UserFileLoadHandle::Contains() const
{
    return pData != nullptr && dataSize > 0;
}



RTGL1::UserFileLoad::UserFileLoad( PFN_rgOpenFile  _openFileFunc,
                                   PFN_rgCloseFile _closeFileFunc,
                                   void*           _pUserData )
    : openFileFunc( _openFileFunc ), closeFileFunc( _closeFileFunc ), pUserData( _pUserData )
{
}

bool RTGL1::UserFileLoad::Exists() const
{
    return openFileFunc != nullptr && closeFileFunc != nullptr;
}

RTGL1::UserFileLoad::UserFileLoadHandle RTGL1::UserFileLoad::Open( const char* pFilePath ) const
{
    return UserFileLoadHandle( this, pFilePath );
}

void RTGL1::UserFileLoad::OpenFile( const char*  pFilePath,
                                    const void** ppOutData,
                                    uint32_t*    pOutDataSize,
                                    void**       ppOutFileUserHandle ) const
{
    *ppOutData    = nullptr;
    *pOutDataSize = 0;

    if( Exists() )
    {
        openFileFunc( pFilePath, pUserData, ppOutData, pOutDataSize, ppOutFileUserHandle );
    }
}

void RTGL1::UserFileLoad::CloseFile( void* pFileUserHandle ) const
{
    if( Exists() )
    {
        closeFileFunc( pFileUserHandle, pUserData );
    }
}
