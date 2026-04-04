#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

class VulkanSwapchain {
public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
              u32 width, u32 height, u32 graphicsFamily, u32 presentFamily);
    void shutdown(VkDevice device);
    void recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                  u32 width, u32 height, u32 graphicsFamily, u32 presentFamily);

    VkSwapchainKHR   getSwapchain()   const { return m_swapchain; }
    VkRenderPass     getRenderPass()  const { return m_renderPass; }
    VkExtent2D       getExtent()      const { return m_extent; }
    VkFormat         getImageFormat() const { return m_imageFormat; }
    VkFramebuffer    getFramebuffer(u32 index) const { return m_framebuffers[index]; }
    u32              getImageCount()  const { return static_cast<u32>(m_images.size()); }

private:
    SwapchainSupportDetails querySupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height);
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                         u32 width, u32 height, u32 graphicsFamily, u32 presentFamily);
    void createImageViews(VkDevice device);
    void createDepthResources(VkPhysicalDevice physicalDevice, VkDevice device);
    void createRenderPass(VkDevice device);
    void createFramebuffers(VkDevice device);
    void cleanup(VkDevice device);

    VkSwapchainKHR             m_swapchain  = VK_NULL_HANDLE;
    VkRenderPass               m_renderPass = VK_NULL_HANDLE;
    VkFormat                   m_imageFormat;
    VkFormat                   m_depthFormat;
    VkExtent2D                 m_extent;
    std::vector<VkImage>       m_images;
    std::vector<VkImageView>   m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;

    // Depth buffer
    VkImage        m_depthImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView    m_depthImageView   = VK_NULL_HANDLE;
};

} // namespace Genesis
