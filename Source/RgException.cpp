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

static const char *GetRgResultName(RgResult r)
{
    switch (r)
    {
        case RG_SUCCESS: return "RG_SUCCESS";
        case RG_GRAPHICS_API_ERROR: return "RG_GRAPHICS_API_ERROR";
        case RG_CANT_FIND_PHYSICAL_DEVICE: return "RG_CANT_FIND_PHYSICAL_DEVICE";
        case RG_WRONG_ARGUMENT: return "RG_WRONG_ARGUMENT";
        case RG_TOO_MANY_INSTANCES: return "RG_TOO_MANY_INSTANCES";
        case RG_WRONG_INSTANCE: return "RG_WRONG_INSTANCE";
        case RG_FRAME_WASNT_STARTED: return "RG_FRAME_WASNT_STARTED";
        case RG_FRAME_WASNT_ENDED: return "RG_FRAME_WASNT_ENDED";
        case RG_CANT_UPDATE_TRANSFORM: return "RG_CANT_UPDATE_TRANSFORM";
        case RG_CANT_UPDATE_TEXCOORDS: return "RG_CANT_UPDATE_TEXCOORDS";
        case RG_CANT_UPDATE_DYNAMIC_MATERIAL: return "RG_CANT_UPDATE_DYNAMIC_MATERIAL";
        case RG_CANT_UPDATE_ANIMATED_MATERIAL: return "RG_CANT_UPDATE_ANIMATED_MATERIAL";
        case RG_CANT_UPLOAD_RASTERIZED_GEOMETRY: return "RG_CANT_UPLOAD_RASTERIZED_GEOMETRY";
        case RG_WRONG_MATERIAL_PARAMETER: return "RG_WRONG_MATERIAL_PARAMETER";
        case RG_WRONG_FUNCTION_CALL: return "RG_WRONG_FUNCTION_CALL";
        case RG_TOO_MANY_SECTORS: return "RG_TOO_MANY_SECTORS";
        case RG_ERROR_INCORRECT_SECTOR: return "RG_ERROR_INCORRECT_SECTOR";
        case RG_ERROR_INTERNAL: return "RG_ERROR_INTERNAL";
    }

    return "Unknown RgResult";
}

RTGL1::RgException::RgException(RgResult _errorCode)
    : runtime_error(GetRgResultName(_errorCode)), errorCode(_errorCode)
{
    assert(errorCode != RG_SUCCESS);
}

RTGL1::RgException::RgException(RgResult _errorCode, const std::string &_Message)
    : runtime_error(_Message), errorCode(_errorCode)
{
    assert(errorCode != RG_SUCCESS);
}

RTGL1::RgException::RgException(RgResult _errorCode, const char *_Message)
    : runtime_error(_Message), errorCode(_errorCode)
{
    assert(errorCode != RG_SUCCESS);
}

RgResult RTGL1::RgException::GetErrorCode() const
{
    return errorCode;
}
