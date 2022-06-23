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

#include "LightGrid.h"

#include "CmdLabel.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

RTGL1::LightGrid::LightGrid(
    VkDevice _device,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<Framebuffers> &_framebuffers,
    const std::shared_ptr<BlueNoise> &_blueNoise,
    const std::shared_ptr<LightManager> &_lightManager
)
    : device(_device)
    , pipelineLayout(VK_NULL_HANDLE)
    , gridBuildPipeline(VK_NULL_HANDLE)
{
    VkDescriptorSetLayout setLayouts[] =
    {
        _uniform->GetDescSetLayout(),
        _framebuffers->GetDescSetLayout(),
        _blueNoise->GetDescSetLayout(),
        _lightManager->GetDescSetLayout()
    };

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = std::size(setLayouts);
    layoutInfo.pSetLayouts = setLayouts;

    VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Light grid pipeline layout");


    CreatePipelines(_shaderManager.get());
}

RTGL1::LightGrid::~LightGrid()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
}

void RTGL1::LightGrid::Build(
    VkCommandBuffer cmd, uint32_t frameIndex, 
    const std::shared_ptr<GlobalUniform> &uniform,
    const std::shared_ptr<Framebuffers> &framebuffers, 
    const std::shared_ptr<BlueNoise> &blueNoise,
    const std::shared_ptr<LightManager> &lightManager)
{
    typedef FramebufferImageIndex FI;
    CmdLabel label(cmd, "Light grid build");


    // no barriers here, as lightManager has a AutoBuffer kludge


    VkDescriptorSet sets[] =
    {
        uniform->GetDescSet(frameIndex),
        framebuffers->GetDescSet(frameIndex),
        blueNoise->GetDescSet(),
        lightManager->GetDescSet(frameIndex),
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout,
        0, std::size(sets), sets,
        0, nullptr);


    uint32_t lightSamplesCount = LIGHT_GRID_CELL_SIZE * LIGHT_GRID_SIZE_HORIZONTAL_X * LIGHT_GRID_SIZE_VERTICAL_Y * LIGHT_GRID_SIZE_HORIZONTAL_Z;
    uint32_t wgCountX = Utils::GetWorkGroupCount(lightSamplesCount, COMPUTE_LIGHT_GRID_GROUP_SIZE_X);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gridBuildPipeline);
    vkCmdDispatch(cmd, wgCountX, 1, 1);
}

void RTGL1::LightGrid::OnShaderReload(const ShaderManager* shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::LightGrid::CreatePipelines(const ShaderManager* shaderManager)
{
    VkComputePipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    plInfo.layout = pipelineLayout;
    plInfo.stage = shaderManager->GetStageInfo("CLightGridBuild");

    VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &gridBuildPipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, gridBuildPipeline, VK_OBJECT_TYPE_PIPELINE, "Light grid build pipeline");
}

void RTGL1::LightGrid::DestroyPipelines()
{
    vkDestroyPipeline(device, gridBuildPipeline, nullptr);
    gridBuildPipeline = VK_NULL_HANDLE;

}
