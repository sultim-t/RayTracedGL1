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

#include "ShaderManager.h"

#include <fstream>
#include <vector>

using namespace RTGL1;

struct ShaderModuleDefinition
{
    const char *name;
    const char *path;
    VkShaderStageFlagBits stage;
};

// TODO: move this to separate file
// Note: set shader stage to VK_SHADER_STAGE_ALL, to identify stage by the file extension
static ShaderModuleDefinition G_SHADERS[] =
{
    {"RGenPrimary",         "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenPrimary.rgen.spv"     , VK_SHADER_STAGE_ALL },
    {"RGenDirect",          "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenDirect.rgen.spv"      , VK_SHADER_STAGE_ALL },
    {"RGenIndirect",        "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenIndirect.rgen.spv"    , VK_SHADER_STAGE_ALL },
    {"RMiss",               "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtMiss.rmiss.spv"             , VK_SHADER_STAGE_ALL },
    {"RMissShadow",         "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtMissShadowCheck.rmiss.spv"  , VK_SHADER_STAGE_ALL },
    {"RClsOpaque",          "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtClsOpaque.rchit.spv"        , VK_SHADER_STAGE_ALL },
    {"RAlphaTest",          "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtAlphaTest.rahit.spv"        , VK_SHADER_STAGE_ALL },
    {"RBlendAdditive",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtBlendAdditive.rahit.spv"    , VK_SHADER_STAGE_ALL },
    {"RBlendUnder",         "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtBlendUnder.rahit.spv"       , VK_SHADER_STAGE_ALL },
    {"CComposition",        "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmComposition.comp.spv"       , VK_SHADER_STAGE_ALL },
    {"CLuminanceHistogram", "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmLuminanceHistogram.comp.spv", VK_SHADER_STAGE_ALL },
    {"CLuminanceAvg",       "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmLuminanceAvg.comp.spv"      , VK_SHADER_STAGE_ALL },
    {"RasterizerVert",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/Rasterizer.vert.spv"          , VK_SHADER_STAGE_ALL },
    {"RasterizerFrag",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/Rasterizer.frag.spv"          , VK_SHADER_STAGE_ALL },
};


ShaderManager::ShaderManager(VkDevice device)
{
    this->device = device;
    LoadShaderModules();
}

ShaderManager::~ShaderManager()
{
    UnloadShaderModules();
}

void ShaderManager::ReloadShaders()
{
    UnloadShaderModules();
    LoadShaderModules();
}

void ShaderManager::LoadShaderModules()
{
    for (auto &s : G_SHADERS)
    {
        if (s.stage == VK_SHADER_STAGE_ALL)
        {
            // parse stage if needed, it's done only once, as paths won't be changing
            s.stage = GetStageByExtension(s.path);
        }

        VkShaderModule m = LoadModuleFromFile(s.path);
        SET_DEBUG_NAME(device, m, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, s.name);

        modules[s.name] = { m, s.stage };

    }
}

void ShaderManager::UnloadShaderModules()
{
    for (auto &s: modules)
    {
        vkDestroyShaderModule(device, s.second.module, nullptr);
    }

    modules.clear();
}

VkShaderModule ShaderManager::GetShaderModule(const char* name) const
{
    const auto &m = modules.find(name);
    return m != modules.end() ? m->second.module : VK_NULL_HANDLE;
}

VkShaderStageFlagBits ShaderManager::GetModuleStage(const char* name) const
{
    const auto &m = modules.find(name);
    return m != modules.end() ? m->second.shaderStage : static_cast<VkShaderStageFlagBits>(0);
}

VkPipelineShaderStageCreateInfo ShaderManager::GetStageInfo(const char* name) const
{
    const auto &m = modules.find(name);

    if (m == modules.end())
    {
        assert(0);
        return {};
    }

    VkPipelineShaderStageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.module = m->second.module;
    info.stage = m->second.shaderStage;
    info.pName = "main";

    return info;
}

VkShaderModule ShaderManager::LoadModuleFromFile(const char* path)
{
    std::ifstream shaderFile(path, std::ios::binary);
    std::vector<uint8_t> shaderSource(std::istreambuf_iterator<char>(shaderFile), {});

    assert(!shaderSource.empty() && "Can't find shader file");

    VkShaderModule shaderModule;

    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = shaderSource.size();
    moduleInfo.pCode = (uint32_t *) shaderSource.data();

    VkResult r = vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule);
    VK_CHECKERROR(r);

    return shaderModule;
}

VkShaderStageFlagBits ShaderManager::GetStageByExtension(const char *name)
{
    // assume that file names end with ".spv"

    if (std::strstr(name, ".vert.spv") != nullptr)
    {
        return VK_SHADER_STAGE_VERTEX_BIT;
    }
    else if(std::strstr(name, ".frag.spv") != nullptr)
    {
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    else if (std::strstr(name, ".comp.spv") != nullptr)
    {
        return VK_SHADER_STAGE_COMPUTE_BIT;
    }
    else if (std::strstr(name, ".rgen.spv") != nullptr)
    {
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }
    else if (std::strstr(name, ".rahit.spv") != nullptr)
    {
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    }
    else if (std::strstr(name, ".rchit.spv") != nullptr)
    {
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    }
    else if (std::strstr(name, ".rmiss.spv") != nullptr)
    {
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    }
    else if (std::strstr(name, ".rcall.spv") != nullptr)
    {
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    }
    else if (std::strstr(name, ".rint.spv") != nullptr)
    {
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    }
    else if (std::strstr(name, ".tesc.spv") != nullptr)
    {
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }
    else if (std::strstr(name, ".tese.spv") != nullptr)
    {
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    else if (std::strstr(name, ".mesh.spv") != nullptr)
    {
        return VK_SHADER_STAGE_MESH_BIT_NV;
    }
    else if (std::strstr(name, ".task.spv") != nullptr)
    {
        return VK_SHADER_STAGE_TASK_BIT_NV;
    }

    assert(0);
    return VK_SHADER_STAGE_ALL;
}
