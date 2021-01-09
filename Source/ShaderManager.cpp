#include "ShaderManager.h"

#include <fstream>
#include <vector>

struct ShaderModuleDefinition
{
    const char *name;
    const char *path;
    VkShaderStageFlagBits stage;
};

// TODO: move this to separate file
static ShaderModuleDefinition G_SHADERS[] =
{
    {"RGen",            "../../../BasicRaygen.rgen.spv",            VK_SHADER_STAGE_RAYGEN_BIT_KHR },
    {"RMiss",           "../../../BasicMiss.rmiss.spv",             VK_SHADER_STAGE_MISS_BIT_KHR },
    {"RMissShadow",     "../../../BasicShadowCheck.rmiss.spv",      VK_SHADER_STAGE_MISS_BIT_KHR },
    {"RClsHit",         "../../../BasicClosestHit.rchit.spv",       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
    {"RasterizerVert",  "../../../Rasterizer.vert.spv",             VK_SHADER_STAGE_VERTEX_BIT },
    {"RasterizerFrag",  "../../../Rasterizer.frag.spv",             VK_SHADER_STAGE_FRAGMENT_BIT },
};
static const uint32_t G_SHADERS_COUNT = sizeof(G_SHADERS) / sizeof(G_SHADERS[0]);


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
    for (uint32_t i = 0; i < G_SHADERS_COUNT; i++)
    {
        VkShaderModule m = LoadModuleFromFile(G_SHADERS[i].path);
        SET_DEBUG_NAME(device, m, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, G_SHADERS[i].name);

        modules[G_SHADERS[i].name] = { m, G_SHADERS[i].stage };

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
    assert(!shaderSource.empty());

    VkShaderModule shaderModule;

    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = shaderSource.size();
    moduleInfo.pCode = (uint32_t *) shaderSource.data();

    VkResult r = vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule);
    VK_CHECKERROR(r);

    return shaderModule;
}
