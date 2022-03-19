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
#include <cstring>
#include "RgException.h"

using namespace RTGL1;

struct ShaderModuleDefinition
{
    const char *name = nullptr;
    const char *filename = nullptr;
    // will be parsed from filename once
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_ALL;
};

// Note: set shader stage to VK_SHADER_STAGE_ALL, to identify stage by the file extension
static ShaderModuleDefinition G_SHADERS[] =
{
    {"RGenPrimary",             "RtRaygenPrimary.rgen.spv"             },
    {"RGenReflRefr",            "RtRaygenReflRefr.rgen.spv"            },
    {"RGenDirect",              "RtRaygenDirect.rgen.spv"              },
    {"RGenIndirect",            "RtRaygenIndirect.rgen.spv"            },
    {"RMiss",                   "RtMiss.rmiss.spv"                     },
    {"RMissShadow",             "RtMissShadowCheck.rmiss.spv"          },
    {"RClsOpaque",              "RtClsOpaque.rchit.spv"                },
    {"RAlphaTest",              "RtAlphaTest.rahit.spv"                },
    {"CComposition",            "CmComposition.comp.spv"               },
    {"CLuminanceHistogram",     "CmLuminanceHistogram.comp.spv"        },
    {"CLuminanceAvg",           "CmLuminanceAvg.comp.spv"              },
    {"VertRasterizer",          "RsRasterizer.vert.spv"                },
    {"FragRasterizer",          "RsRasterizer.frag.spv"                },
    {"VertRasterizerMultiview", "RsRasterizerMultiview.vert.spv"       },
    {"VertFullscreenQuad",      "RsFullscreenQuad.vert.spv"            },
    {"FragDepthCopying",        "RsDepthCopying.frag.spv"              },
    {"CVertexPreprocess",       "CmVertexPreprocess.comp.spv"          },
    {"CSVGFTemporalAccum",      "CmSVGFTemporalAccumulation.comp.spv"  },
    {"CSVGFVarianceEstim",      "CmSVGFEstimateVariance.comp.spv"      },
    {"CSVGFAtrous",             "CmSVGFAtrous.comp.spv"                },
    {"CSVGFAtrous_Iter0",       "CmSVGFAtrous_Iter0.comp.spv"          },
    {"CASVGFMerging",           "CmASVGFMerging.comp.spv"              },
    {"CASVGFGradientSamples",   "CmASVGFGradientSamples.comp.spv"      },
    {"CASVGFGradientAtrous",    "CmASVGFGradientAtrous.comp.spv"       },
    {"CBloomDownsample",        "CmBloomDownsample.comp.spv"           },
    {"CBloomUpsample",          "CmBloomUpsample.comp.spv"             },
    {"CBloomApply",             "CmBloomApply.comp.spv"                },
    {"CCheckerboard",           "CmCheckerboard.comp.spv"              },
    {"CFsrEasu",                "CmFsrEasu.comp.spv"                   },
    {"CFsrRcas",                "CmFsrRcas.comp.spv"                   },
    {"CCas",                    "CmCas.comp.spv"                       },
    {"VertLensFlare",           "RsRasterizerLensFlare.vert.spv"       },
    {"FragLensFlare",           "RsRasterizerLensFlare.frag.spv"       },
    {"CCullLensFlares",         "CmCullLensFlares.comp.spv"            },
    {"VertDecal",               "RsDecal.vert.spv"                     },
    {"FragDecal",               "RsDecal.frag.spv"                     },
    {"EffectWipe",                  "EfWipe.comp.spv"                  },
    {"EffectRadialBlur",            "EfRadialBlur.comp.spv"            },
    {"EffectChromaticAberration",   "EfChromaticAberration.comp.spv"   },
    {"EffectInverseBW",             "EfInverseBW.comp.spv"             },
    {"EffectDistortedSides",        "EfDistortedSides.comp.spv"        },
    {"EffectColorTint",             "EfColorTint.comp.spv"             },
    {"EffectHueShift",              "EfHueShift.comp.spv"             },
};


ShaderManager::ShaderManager(VkDevice _device, const char *_pShaderFolderPath, std::shared_ptr<UserFileLoad> _userFileLoad)
    : device(_device), userFileLoad(std::move(_userFileLoad)), shaderFolderPath(_pShaderFolderPath)
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
        assert(s.filename != nullptr);
        assert(s.name != nullptr);

        if (s.stage == VK_SHADER_STAGE_ALL)
        {
            // parse stage if needed, it's done only once, as names won't be changing
            s.stage = GetStageByExtension(s.filename);
        }

        auto path = shaderFolderPath + s.filename;

        VkShaderModule m = LoadModule(path.c_str());
        SET_DEBUG_NAME(device, m, VK_OBJECT_TYPE_SHADER_MODULE, s.name);

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

VkPipelineShaderStageCreateInfo ShaderManager::GetStageInfo(const char *name) const
{
    const auto &m = modules.find(name);

    if (m == modules.end())
    {
        using namespace std::string_literals;

        throw RgException(RG_WRONG_ARGUMENT, "Can't find loaded shader with name \""s + name + "\"");
        return {};
    }

    VkPipelineShaderStageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.module = m->second.module;
    info.stage = m->second.shaderStage;
    info.pName = "main";

    return info;
}

VkShaderModule RTGL1::ShaderManager::LoadModule(const char *path)
{
    if (userFileLoad->Exists())
    {
        auto fileHandle = userFileLoad->Open(path);

        if (!fileHandle.Contains())
        {
            using namespace std::string_literals;
            throw RgException(RG_WRONG_ARGUMENT, "Can't load shader file \""s + path + "\" using user's file load function"s);
        }

        return LoadModuleFromMemory(static_cast<const uint32_t*>(fileHandle.pData), fileHandle.dataSize);
    }
    else
    {
        return LoadModuleFromFile(path);
    }
}

VkShaderModule ShaderManager::LoadModuleFromFile(const char *path)
{
    std::ifstream shaderFile(path, std::ios::binary);
    std::vector<uint8_t> shaderSource(std::istreambuf_iterator<char>(shaderFile), {});

    if (shaderSource.empty())
    {
        using namespace std::string_literals;
        throw RgException(RG_WRONG_ARGUMENT, "Can't find shader file: \""s + path + "\"");
    }

    return LoadModuleFromMemory(reinterpret_cast<const uint32_t*>(shaderSource.data()), shaderSource.size());
}

VkShaderModule ShaderManager::LoadModuleFromMemory(const uint32_t *pCode, uint32_t codeSize)
{
    VkShaderModule shaderModule;

    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = codeSize;
    moduleInfo.pCode = pCode;

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
