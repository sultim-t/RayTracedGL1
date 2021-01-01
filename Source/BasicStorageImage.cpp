#include "BasicStorageImage.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

BasicStorageImage::BasicStorageImage(
    VkDevice device,
    std::shared_ptr<PhysicalDevice> physDevice,
    std::shared_ptr<CommandBufferManager> cmdManager) :
    image(VK_NULL_HANDLE),
    imageLayout(VK_IMAGE_LAYOUT_UNDEFINED),
    width(0),height(0),
    view(VK_NULL_HANDLE),
    memory(VK_NULL_HANDLE)
{
    this->device = device;
    this->physDevice = physDevice;
    this->cmdManager = cmdManager;

    CreateDescriptors();
}

BasicStorageImage::~BasicStorageImage()
{
    DestroyImage();

    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
}

void BasicStorageImage::CreateImage(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;

    VkResult r;
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    r = vkCreateImage(device, &imageInfo, nullptr, &image);
    VK_CHECKERROR(r);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    memory = physDevice->AllocDeviceMemory(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    r = vkBindImageMemory(device, image, memory, 0);
    VK_CHECKERROR(r);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.image = image;
    r = vkCreateImageView(device, &viewInfo, nullptr, &view);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, image, VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Output image");
    SET_DEBUG_NAME(device, view, VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Output image View");

    // TODO: cmd should be created here 
    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    Utils::BarrierImage(
        cmd, image,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    cmdManager->Submit(cmd);

    UpdateDescriptors();
}

void BasicStorageImage::DestroyImage()
{
    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, image, nullptr);
        vkDestroyImageView(device, view, nullptr);
        physDevice->FreeDeviceMemory(memory);

        image = VK_NULL_HANDLE;
        imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        view = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
}

void BasicStorageImage::Barrier(VkCommandBuffer cmd)
{
    Utils::BarrierImage(
        cmd, image,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL);
}

void BasicStorageImage::OnSwapchainCreate(uint32_t newWidth, uint32_t newHeight)
{
    CreateImage(newWidth, newHeight);
}

void BasicStorageImage::OnSwapchainDestroy()
{
    DestroyImage();
}

void BasicStorageImage::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding storageImageBinding = {};
    storageImageBinding.binding = BINDING_STORAGE_IMAGE;
    storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    storageImageBinding.descriptorCount = 1;
    storageImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &storageImageBinding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &allocInfo, &descSets[i]);
        VK_CHECKERROR(r);
        SET_DEBUG_NAME(device, descSets[i], VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "Storage image Desc set");
    }

    SET_DEBUG_NAME(device, descLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Storage image Desc set Layout");
}

void BasicStorageImage::UpdateDescriptors()
{
    VkDescriptorImageInfo imageInfos[MAX_FRAMES_IN_FLIGHT] = {};
    VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT] = {};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        imageInfos[i].imageView = view;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descSets[i];
        writes[i].dstBinding = BINDING_STORAGE_IMAGE;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }

    vkUpdateDescriptorSets(device, MAX_FRAMES_IN_FLIGHT, writes, 0, nullptr);
}
