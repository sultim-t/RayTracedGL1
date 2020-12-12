#pragma once

#include <map>
#include <string>

#include "Common.h"

// This class provides shader modules by their name
class ShaderManager
{
public:
    explicit ShaderManager(VkDevice device);
    ~ShaderManager();

    ShaderManager(const ShaderManager& other) = delete;
    ShaderManager(ShaderManager&& other) noexcept = delete;
    ShaderManager& operator=(const ShaderManager& other) = delete;
    ShaderManager& operator=(ShaderManager&& other) noexcept = delete;

    void ReloadShaders();

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
    VkShaderModule LoadModuleFromFile(const char *path);
    void LoadShaderModules();
    void UnloadShaderModules();

private:
    VkDevice device;

    std::map<std::string, ShaderModule> modules;
};