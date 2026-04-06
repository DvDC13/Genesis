#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>

namespace Genesis {

class ViewportFramebuffer {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              u32 width, u32 height, VkFormat colorFormat);
    void shutdown(VkDevice device);

    // Recreate at a new size (called when viewport panel resizes)
    void resize(VkDevice device, VkPhysicalDevice physicalDevice,
                u32 width, u32 height);

    // Register the color attachment with ImGui for display (call once after init)
    void createImGuiDescriptor();
    // Re-register after resize
    void updateImGuiDescriptor();

    VkRenderPass    getRenderPass()  const { return m_renderPass; }
    VkFramebuffer   getFramebuffer() const { return m_framebuffer; }
    VkExtent2D      getExtent()      const { return { m_width, m_height }; }
    VkDescriptorSet getImGuiTexture() const { return m_imguiDescSet; }
    VkFormat        getColorFormat()  const { return m_colorFormat; }

private:
    void createResources(VkDevice device, VkPhysicalDevice physicalDevice);
    void destroyResources(VkDevice device);
    void createRenderPass(VkDevice device);

    // Color attachment
    VkImage        m_colorImage  = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkImageView    m_colorView   = VK_NULL_HANDLE;
    VkSampler      m_sampler     = VK_NULL_HANDLE;

    // Depth attachment
    VkImage        m_depthImage  = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
    VkImageView    m_depthView   = VK_NULL_HANDLE;

    VkRenderPass    m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer   m_framebuffer = VK_NULL_HANDLE;
    VkFormat        m_colorFormat = VK_FORMAT_B8G8R8A8_SRGB;

    // ImGui texture handle
    VkDescriptorSet m_imguiDescSet = VK_NULL_HANDLE;

    u32 m_width  = 0;
    u32 m_height = 0;
};

} // namespace Genesis
