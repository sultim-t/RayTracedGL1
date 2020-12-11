#pragma once

#include <vector>
#include <map>
#include <string>

#include "Common.h"

class ShaderManager
{
public:
    explicit ShaderManager(VkDevice device);
    ~ShaderManager();

    void LoadShaderModules();
    void UnloadShaderModules();

    VkShaderModule GetShaderModule(const char *name) const;
    VkShaderStageFlagBits GetModuleStage(const char *name) const;
    VkPipelineShaderStageCreateInfo GetStageInfo(const char *name) const;

private:
    struct ShaderModule
    {
        VkShaderModule module;
        VkShaderStageFlagBits shaderStage;
    };

private:
    VkShaderModule LoadModule(const char *path);

private:
    VkDevice device;

    std::map<std::string, ShaderModule> modules;
};