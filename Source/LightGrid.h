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

#include "ShaderManager.h"
#include "GlobalUniform.h"
#include "BlueNoise.h"
#include "LightManager.h"

namespace RTGL1
{
    class LightGrid : public IShaderDependency
    {
    public:
        LightGrid(
            VkDevice device,
            const std::shared_ptr<ShaderManager> &shaderManager,
            const std::shared_ptr<GlobalUniform> &uniform,
            const std::shared_ptr<BlueNoise> &blueNoise,
            const std::shared_ptr<LightManager> &lightManager);
        ~LightGrid() override;

        LightGrid(const LightGrid &other) = delete;
        LightGrid(LightGrid &&other) noexcept = delete;
        LightGrid &operator=(const LightGrid &other) = delete;
        LightGrid &operator=(LightGrid &&other) noexcept = delete;

        void Build(
            VkCommandBuffer cmd, uint32_t frameIndex, 
            const std::shared_ptr<GlobalUniform> &uniform,
            const std::shared_ptr<BlueNoise> &blueNoise,
            const std::shared_ptr<LightManager> &lightManager);

        void OnShaderReload(const ShaderManager *shaderManager) override;

    private:
        void CreatePipelines(const ShaderManager *shaderManager);
        void DestroyPipelines();

    private:
        VkDevice device;

        VkPipelineLayout pipelineLayout;
        VkPipeline gridBuildPipeline;

    };
}