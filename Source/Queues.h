#pragma once

#include <vector>
#include "Common.h"

class Queues
{
public:
    explicit Queues(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
    ~Queues();

    Queues(const Queues& other) = delete;
    Queues(Queues&& other) noexcept = delete;
    Queues& operator=(const Queues& other) = delete;
    Queues& operator=(Queues&& other) noexcept = delete;

    void SetDevice(VkDevice device);

    void GetDeviceQueueCreateInfos(std::vector<VkDeviceQueueCreateInfo> &outInfos) const;

    uint32_t GetIndexGraphics() const;
    uint32_t GetIndexCompute() const;
    uint32_t GetIndexTransfer() const;
    VkQueue GetGraphics() const;
    VkQueue GetCompute() const;
    VkQueue GetTransfer() const;

private:
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;

    uint32_t indexGraphics;
    uint32_t indexCompute;
    uint32_t indexTransfer;

    VkQueue graphics;
    VkQueue compute;
    VkQueue transfer;
};