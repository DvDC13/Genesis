#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <string>

namespace Genesis {

class VulkanTexture {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkCommandPool commandPool, VkQueue queue,
              const std::string& filepath);

    void initDefault(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool, VkQueue queue);

    void initCheckerboard(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue queue,
                          u32 size = 256, u32 squares = 8);

    void initGradient(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue queue,
                      u32 size = 256);

    void shutdown(VkDevice device);

    VkImageView getImageView() const { return m_imageView; }
    VkSampler   getSampler()   const { return m_sampler; }

private:
    void createImage(VkDevice device, VkPhysicalDevice physicalDevice,
                     u32 width, u32 height, VkFormat format,
                     VkImageUsageFlags usage);
    void transitionImageLayout(VkDevice device, VkCommandPool commandPool,
                               VkQueue queue, VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkCommandPool commandPool,
                           VkQueue queue, VkBuffer buffer,
                           u32 width, u32 height);
    void createImageView(VkDevice device, VkFormat format);
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice);

    VkImage        m_image      = VK_NULL_HANDLE;
    VkDeviceMemory m_memory     = VK_NULL_HANDLE;
    VkImageView    m_imageView  = VK_NULL_HANDLE;
    VkSampler      m_sampler    = VK_NULL_HANDLE;
    u32            m_width      = 0;
    u32            m_height     = 0;
};

} // namespace Genesis
