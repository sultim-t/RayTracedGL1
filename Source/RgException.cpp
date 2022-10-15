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

#include "RgException.h"

#include <cassert>

namespace
{

bool IsSuccess( RgResult r ) {
    return r == RG_RESULT_SUCCESS || r == RG_RESULT_SUCCESS_FOUND_MESH ||
           r == RG_RESULT_SUCCESS_FOUND_TEXTURE;
}

}

RTGL1::RgException::RgException( RgResult _errorCode )
    : runtime_error( GetRgResultName( _errorCode ) ), errorCode( _errorCode )
{
    assert( !IsSuccess( errorCode ) );
}

RTGL1::RgException::RgException( RgResult _errorCode, const std::string& _message )
    : runtime_error( _message ), errorCode( _errorCode )
{
    assert( !IsSuccess( errorCode ) );
}

RTGL1::RgException::RgException( RgResult _errorCode, const char* _message )
    : runtime_error( _message ), errorCode( _errorCode )
{
    assert( !IsSuccess( errorCode ) );
}

RgResult RTGL1::RgException::GetErrorCode() const
{
    return errorCode;
}

const char* RTGL1::RgException::GetRgResultName( RgResult r )
{
    switch( r )
    {
        case RG_RESULT_SUCCESS: return "RG_RESULT_SUCCESS";
        case RG_RESULT_SUCCESS_FOUND_MESH: return "RG_RESULT_SUCCESS_FOUND_MESH";
        case RG_RESULT_SUCCESS_FOUND_TEXTURE: return "RG_RESULT_SUCCESS_FOUND_TEXTURE";
        case RG_RESULT_WRONG_INSTANCE: return "RG_RESULT_WRONG_INSTANCE";
        case RG_RESULT_GRAPHICS_API_ERROR: return "RG_RESULT_GRAPHICS_API_ERROR";
        case RG_RESULT_CANT_FIND_SUPPORTED_PHYSICAL_DEVICE: return "RG_RESULT_CANT_FIND_SUPPORTED_PHYSICAL_DEVICE";
        case RG_RESULT_FRAME_WASNT_STARTED: return "RG_RESULT_FRAME_WASNT_STARTED";
        case RG_RESULT_FRAME_WASNT_ENDED: return "RG_RESULT_FRAME_WASNT_ENDED";
        case RG_RESULT_WRONG_FUNCTION_CALL: return "RG_RESULT_WRONG_FUNCTION_CALL";
        case RG_RESULT_WRONG_FUNCTION_ARGUMENT: return "RG_RESULT_WRONG_FUNCTION_ARGUMENT";
        case RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES: return "RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES";
        case RG_RESULT_ERROR_CANT_FIND_SHADER: return "RG_RESULT_ERROR_CANT_FIND_SHADER";
        default: assert( 0 ); return "Unknown RgResult";
    }
}
