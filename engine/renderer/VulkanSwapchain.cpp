#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanBuffer.h"
#include "core/Logger.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Genesis {

void VulkanSwapchain::init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                           u32 width, u32 height, u32 graphicsFamily, u32 presentFamily) {
    createSwapchain(physicalDevice, device, surface, width, height, graphicsFamily, presentFamily);
    createImageViews(device);
    createDepthResources(physicalDevice, device);
    createRenderPass(device);
    createFramebuffers(device);
}

void VulkanSwapchain::shutdown(VkDevice device) {
    cleanup(device);
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    Logger::info("Swapchain destroyed");
}

void VulkanSwapchain::recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                               u32 width, u32 height, u32 graphicsFamily, u32 presentFamily) {
    cleanup(device);
    createSwapchain(physicalDevice, device, surface, width, height, graphicsFamily, presentFamily);
    createImageViews(device);
    createDepthResources(physicalDevice, device);
    createFramebuffers(device);
    Logger::info("Swapchain recreated: {}x{}", m_extent.width, m_extent.height);
}

void VulkanSwapchain::createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                                      u32 width, u32 height, u32 graphicsFamily, u32 presentFamily) {
    SwapchainSupportDetails support = querySupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode     = choosePresentMode(support.presentModes);
    VkExtent2D extent                = chooseExtent(support.capabilities, width, height);

    u32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    u32 queueFamilyIndices[] = { graphicsFamily, presentFamily };
    if (graphicsFamily != presentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = m_swapchain;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    Logger::info("Swapchain created: {}x{}, {} images", extent.width, extent.height, imageCount);
}

void VulkanSwapchain::createImageViews(VkDevice device) {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image    = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format   = m_imageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }
}

void VulkanSwapchain::createDepthResources(VkPhysicalDevice physicalDevice, VkDevice device) {
    m_depthFormat = findDepthFormat(physicalDevice);

    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = m_depthFormat;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image");
    }

    // Allocate memory for the depth image
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_depthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_depthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth image memory");
    }

    vkBindImageMemory(device, m_depthImage, m_depthImageMemory, 0);

    // Create depth image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = m_depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view");
    }

    Logger::info("Depth buffer created ({})", m_depthFormat == VK_FORMAT_D32_SFLOAT ? "D32" : "D24S8");
}

void VulkanSwapchain::createRenderPass(VkDevice device) {
    // Color attachment (index 0)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_imageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment (index 1)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = m_depthFormat;
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

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments    = attachments;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
    Logger::info("Render pass created (color + depth)");
}

void VulkanSwapchain::createFramebuffers(VkDevice device) {
    m_framebuffers.resize(m_imageViews.size());

    for (size_t i = 0; i < m_imageViews.size(); i++) {
        VkImageView attachments[] = { m_imageViews[i], m_depthImageView };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = m_extent.width;
        fbInfo.height          = m_extent.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanSwapchain::cleanup(VkDevice device) {
    for (auto fb : m_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    m_framebuffers.clear();

    // Destroy depth resources
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_depthImageMemory, nullptr);
        m_depthImageMemory = VK_NULL_HANDLE;
    }

    for (auto view : m_imageViews) {
        vkDestroyImageView(device, view, nullptr);
    }
    m_imageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

VkFormat VulkanSwapchain::findDepthFormat(VkPhysicalDevice physicalDevice) {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported depth format");
}

SwapchainSupportDetails VulkanSwapchain::querySupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR VulkanSwapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = { width, height };
    extent.width  = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

} // namespace Genesis
