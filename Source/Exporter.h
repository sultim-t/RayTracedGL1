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

#include <RTGL1/RTGL1.h>

#include "Containers.h"

#include <filesystem>
#include <functional>

namespace RTGL1
{
    struct DeepCopyOfPrimitive;

    class Exporter
    {
    public:
        explicit Exporter(
            std::function< void( const char*, RgMessageSeverityFlags ) > debugprint );
        ~Exporter();

        Exporter( const Exporter& other )                = delete;
        Exporter( Exporter&& other ) noexcept            = delete;
        Exporter& operator=( const Exporter& other )     = delete;
        Exporter& operator=( Exporter&& other ) noexcept = delete;

        void AddPrimitive( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive );
        void ExportToFiles( const std::filesystem::path& folder );

    private:
        std::function< void( const char*, RgMessageSeverityFlags ) > debugprint;
        
        rgl::unordered_map< std::string, std::vector< std::shared_ptr< DeepCopyOfPrimitive > > >
            scene;
    };
}