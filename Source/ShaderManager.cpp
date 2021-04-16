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
    const char *name = nullptr;
    const char *path = nullptr;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_ALL;
};

// TODO: move this to separate file
// Note: set shader stage to VK_SHADER_STAGE_ALL, to identify stage by the file extension
static ShaderModuleDefinition G_SHADERS[] =
{
    {"RGenPrimary",             "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenPrimary.rgen.spv"             },
    {"RGenDirect",              "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenDirect.rgen.spv"              },
    {"RGenIndirect",            "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtRaygenIndirect.rgen.spv"            },
    {"RMiss",                   "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtMiss.rmiss.spv"                     },
    {"RMissShadow",             "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtMissShadowCheck.rmiss.spv"          },
    {"RClsOpaque",              "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtClsOpaque.rchit.spv"                },
    {"RAlphaTest",              "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RtAlphaTest.rahit.spv"                },
    {"CComposition",            "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmComposition.comp.spv"               },
    {"CLuminanceHistogram",     "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmLuminanceHistogram.comp.spv"        },
    {"CLuminanceAvg",           "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmLuminanceAvg.comp.spv"              },
    {"VertRasterizer",          "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/Rasterizer.vert.spv"                  },
    {"FragRasterizer",          "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/Rasterizer.frag.spv"                  },
    {"VertRasterizerMultiview", "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/RasterizerMultiview.vert.spv"         },
    {"VertFullscreenQuad",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/FullscreenQuad.vert.spv"              },
    {"FragDepthCopying",        "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/DepthCopying.frag.spv"                },
    {"CVertexPreprocess",       "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmVertexPreprocess.comp.spv"          },
    {"CSVGFTemporalAccum",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmSVGFTemporalAccumulation.comp.spv"  },
    {"CSVGFVarianceEstim",      "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmSVGFEstimateVariance.comp.spv"      },
    {"CSVGFAtrous",             "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmSVGFAtrous.comp.spv"                },
    {"CASVGFMerging",           "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmASVGFMerging.comp.spv"              },
    {"CASVGFGradientSamples",   "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmASVGFGradientSamples.comp.spv"      },
    {"CASVGFGradientAtrous",    "C:/Git/Serious-Engine-RT/Sources/RTGL1/Build/CmASVGFGradientAtrous.comp.spv",      },
};


ShaderManager::ShaderManager(VkDevice _device) : device(_device)
{
    LoadShaderModules();
}

ShaderManager::~ShaderManager()
{
    UnloadShaderModules();
}

void ShaderManager::ReloadShaders()
{
    vkDeviceWaitIdle(device);

    UnloadShaderModules();
    LoadShaderModules();

    NotifySubscribersAboutReload();

    vkDeviceWaitIdle(device);
}

void ShaderManager::LoadShaderModules()
{
    for (auto &s : G_SHADERS)
    {
        assert(s.path != nullptr);
        assert(s.name != nullptr);

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

void ShaderManager::Subscribe(std::shared_ptr<IShaderDependency> subscriber)
{
    subscribers.emplace_back(subscriber);
}

void ShaderManager::Unsubscribe(const IShaderDependency *subscriber)
{
    subscribers.remove_if([subscriber] (const std::weak_ptr<IShaderDependency> &ws)
    {
        if (const auto s = ws.lock())
        {
            return s.get() == subscriber;
        }

        return true;
    });
}

void ShaderManager::NotifySubscribersAboutReload()
{
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            s->OnShaderReload(this);
        }
    }
}
