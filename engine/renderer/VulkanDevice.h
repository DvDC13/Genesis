#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <optional>

namespace Genesis {

struct QueueFamilyIndices {
    std::optional<u32> graphicsFamily;
    std::optional<u32> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanDevice {
public:
    void init(VkInstance instance, VkSurfaceKHR surface);
    void shutdown();

    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice         getDevice()         const { return m_device; }
    VkQueue          getGraphicsQueue()  const { return m_graphicsQueue; }
    VkQueue          getPresentQueue()   const { return m_presentQueue; }
    QueueFamilyIndices getQueueFamilies() const { return m_queueFamilies; }

private:
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    VkPhysicalDevice   m_physicalDevice = VK_NULL_HANDLE;
    VkDevice           m_device         = VK_NULL_HANDLE;
    VkQueue            m_graphicsQueue  = VK_NULL_HANDLE;
    VkQueue            m_presentQueue   = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilies;

    static constexpr const char* DEVICE_EXTENSIONS[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

} // namespace Genesis
