#include "renderer/ViewportFramebuffer.h"
#include "renderer/VulkanBuffer.h"
#include "core/Logger.h"

#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <array>

namespace Genesis {

void ViewportFramebuffer::init(VkDevice device, VkPhysicalDevice physicalDevice,
                                u32 width, u32 height, VkFormat colorFormat) {
    m_width       = width;
    m_height      = height;
    m_colorFormat = colorFormat;

    createRenderPass(device);
    createResources(device, physicalDevice);

    Logger::info("Viewport framebuffer created ({}x{})", width, height);
}

void ViewportFramebuffer::shutdown(VkDevice device) {
    // Remove ImGui descriptor before destroying resources
    if (m_imguiDescSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_imguiDescSet);
        m_imguiDescSet = VK_NULL_HANDLE;
    }

    destroyResources(device);

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    Logger::info("Viewport framebuffer destroyed");
}

void ViewportFramebuffer::resize(VkDevice device, VkPhysicalDevice physicalDevice,
                                  u32 width, u32 height) {
    if (width == m_width && height == m_height) return;
    if (width == 0 || height == 0) return;

    m_width  = width;
    m_height = height;

    destroyResources(device);
    createResources(device, physicalDevice);
    updateImGuiDescriptor();

    Logger::info("Viewport framebuffer resized ({}x{})", width, height);
}

void ViewportFramebuffer::createImGuiDescriptor() {
    m_imguiDescSet = ImGui_ImplVulkan_AddTexture(
        m_sampler, m_colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ViewportFramebuffer::updateImGuiDescriptor() {
    if (m_imguiDescSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_imguiDescSet);
    }
    m_imguiDescSet = ImGui_ImplVulkan_AddTexture(
        m_sampler, m_colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// ─── Private ───

void ViewportFramebuffer::createRenderPass(VkDevice device) {
    // Color attachment — scene renders here, then sampled by ImGui
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_colorFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependency: ensure color writes complete before fragment shader reads (ImGui sampling)
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = 0;
    dependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<u32>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport render pass");
    }
}

void ViewportFramebuffer::createResources(VkDevice device, VkPhysicalDevice physicalDevice) {
    // ─── Color image ───
    VkImageCreateInfo colorInfo{};
    colorInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorInfo.imageType     = VK_IMAGE_TYPE_2D;
    colorInfo.format        = m_colorFormat;
    colorInfo.extent        = { m_width, m_height, 1 };
    colorInfo.mipLevels     = 1;
    colorInfo.arrayLayers   = 1;
    colorInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    colorInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    colorInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &colorInfo, nullptr, &m_colorImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport color image");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_colorImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate viewport color memory");
    }
    vkBindImageMemory(device, m_colorImage, m_colorMemory, 0);

    // Color image view
    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image                           = m_colorImage;
    colorViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format                          = m_colorFormat;
    colorViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel   = 0;
    colorViewInfo.subresourceRange.levelCount     = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &colorViewInfo, nullptr, &m_colorView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport color image view");
    }

    // Sampler for ImGui to sample the color attachment
    if (m_sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create viewport sampler");
        }
    }

    // ─── Depth image ───
    VkImageCreateInfo depthInfo{};
    depthInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType     = VK_IMAGE_TYPE_2D;
    depthInfo.format        = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent        = { m_width, m_height, 1 };
    depthInfo.mipLevels     = 1;
    depthInfo.arrayLayers   = 1;
    depthInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &depthInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport depth image");
    }

    vkGetImageMemoryRequirements(device, m_depthImage, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate viewport depth memory");
    }
    vkBindImageMemory(device, m_depthImage, m_depthMemory, 0);

    // Depth image view
    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image                           = m_depthImage;
    depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format                          = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel   = 0;
    depthViewInfo.subresourceRange.levelCount     = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &depthViewInfo, nullptr, &m_depthView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport depth image view");
    }

    // ─── Framebuffer ───
    std::array<VkImageView, 2> attachments = { m_colorView, m_depthView };

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = m_renderPass;
    fbInfo.attachmentCount = static_cast<u32>(attachments.size());
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = m_width;
    fbInfo.height          = m_height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create viewport framebuffer");
    }
}

void ViewportFramebuffer::destroyResources(VkDevice device) {
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    if (m_depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depthView, nullptr);
        m_depthView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }
    if (m_colorView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_colorView, nullptr);
        m_colorView = VK_NULL_HANDLE;
    }
    if (m_colorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_colorImage, nullptr);
        m_colorImage = VK_NULL_HANDLE;
    }
    if (m_colorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_colorMemory, nullptr);
        m_colorMemory = VK_NULL_HANDLE;
    }
    // Keep m_sampler alive — reused across resizes
}

} // namespace Genesis
