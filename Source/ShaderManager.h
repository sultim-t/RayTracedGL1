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

#include <list>
#include <string>

#include "Common.h"
#include "Containers.h"
#include "IShaderDependency.h"
#include "UserFunction.h"

namespace RTGL1
{

// This class provides shader modules by their name
class ShaderManager
{
public:
    explicit ShaderManager( VkDevice                        device,
                            const char*                     pShaderFolderPath,
                            std::shared_ptr< UserFileLoad > userFileLoad );
    ~ShaderManager();

    ShaderManager( const ShaderManager& other )     = delete;
    ShaderManager( ShaderManager&& other ) noexcept = delete;
    ShaderManager&                  operator=( const ShaderManager& other ) = delete;
    ShaderManager&                  operator=( ShaderManager&& other ) noexcept = delete;

    void                            ReloadShaders();

    VkShaderModule                  GetShaderModule( const char* name ) const;
    VkShaderStageFlagBits           GetModuleStage( const char* name ) const;
    VkPipelineShaderStageCreateInfo GetStageInfo( const char* name ) const;

    // Subscribe to shader reload event.
    // shared_ptr will be transformed to weak_ptr
    void                            Subscribe( std::shared_ptr< IShaderDependency > subscriber );
    void                            Unsubscribe( const IShaderDependency* subscriber );

private:
    struct ShaderModule
    {
        VkShaderModule        module;
        VkShaderStageFlagBits shaderStage;
    };

private:
    static VkShaderStageFlagBits GetStageByExtension( const char* name );

    VkShaderModule               LoadModule( const char* path );
    VkShaderModule               LoadModuleFromFile( const char* path );
    VkShaderModule               LoadModuleFromMemory( const uint32_t* pCode, uint32_t codeSize );
    void                         LoadShaderModules();
    void                         UnloadShaderModules();

    void                         NotifySubscribersAboutReload();

private:
    VkDevice                                        device;
    std::shared_ptr< UserFileLoad >                 userFileLoad;
    std::string                                     shaderFolderPath;

    rgl::unordered_map< std::string, ShaderModule > modules;

    std::list< std::weak_ptr< IShaderDependency > > subscribers;
};

}