#include "renderer/VulkanTexture.h"
#include "renderer/VulkanBuffer.h"
#include "core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>
#include <cstring>

namespace Genesis {

void VulkanTexture::init(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue queue,
                          const std::string& filepath) {
    // Load image from file
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + filepath);
    }

    m_width  = static_cast<u32>(texWidth);
    m_height = static_cast<u32>(texHeight);
    VkDeviceSize imageSize = m_width * m_height * 4;

    // Create staging buffer and copy pixel data into it
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::createBuffer(device, physicalDevice, imageSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    stbi_image_free(pixels);

    // Create the VkImage
    createImage(device, physicalDevice, m_width, m_height, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    // Transition: UNDEFINED → TRANSFER_DST_OPTIMAL
    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy staging buffer to image
    copyBufferToImage(device, commandPool, queue, stagingBuffer, m_width, m_height);

    // Transition: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create image view and sampler
    createImageView(device, VK_FORMAT_R8G8B8A8_SRGB);
    createSampler(device, physicalDevice);

    Logger::info("Texture loaded: {} ({}x{})", filepath, m_width, m_height);
}

void VulkanTexture::initDefault(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkCommandPool commandPool, VkQueue queue) {
    // Create a 1x1 white pixel texture as default
    m_width  = 1;
    m_height = 1;
    uint8_t pixel[4] = { 255, 255, 255, 255 };
    VkDeviceSize imageSize = 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::createBuffer(device, physicalDevice, imageSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixel, 4);
    vkUnmapMemory(device, stagingMemory);

    createImage(device, physicalDevice, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, commandPool, queue, stagingBuffer, 1, 1);

    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    createImageView(device, VK_FORMAT_R8G8B8A8_SRGB);
    createSampler(device, physicalDevice);

    Logger::info("Default white texture created (1x1)");
}

void VulkanTexture::initCheckerboard(VkDevice device, VkPhysicalDevice physicalDevice,
                                      VkCommandPool commandPool, VkQueue queue,
                                      u32 size, u32 squares) {
    m_width  = size;
    m_height = size;
    VkDeviceSize imageSize = size * size * 4;
    u32 squareSize = size / squares;

    // Generate checkerboard pattern
    std::vector<uint8_t> pixels(imageSize);
    for (u32 y = 0; y < size; y++) {
        for (u32 x = 0; x < size; x++) {
            bool isWhite = ((x / squareSize) + (y / squareSize)) % 2 == 0;
            u32 idx = (y * size + x) * 4;
            uint8_t color = isWhite ? 220 : 40;
            pixels[idx + 0] = color;
            pixels[idx + 1] = color;
            pixels[idx + 2] = color;
            pixels[idx + 3] = 255;
        }
    }

    // Upload via staging buffer (same pattern as init/initDefault)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::createBuffer(device, physicalDevice, imageSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    createImage(device, physicalDevice, size, size, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, commandPool, queue, stagingBuffer, size, size);

    transitionImageLayout(device, commandPool, queue,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    createImageView(device, VK_FORMAT_R8G8B8A8_SRGB);
    createSampler(device, physicalDevice);

    Logger::info("Checkerboard texture created ({}x{}, {}x{} squares)", size, size, squares, squares);
}

void VulkanTexture::shutdown(VkDevice device) {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
    }
    Logger::info("Texture destroyed");
}

void VulkanTexture::createImage(VkDevice device, VkPhysicalDevice physicalDevice,
                                 u32 width, u32 height, VkFormat format,
                                 VkImageUsageFlags usage) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture image memory");
    }

    vkBindImageMemory(device, m_image, m_memory, 0);
}

void VulkanTexture::transitionImageLayout(VkDevice device, VkCommandPool commandPool,
                                           VkQueue queue, VkImageLayout oldLayout,
                                           VkImageLayout newLayout) {
    // Allocate a one-shot command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = m_image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void VulkanTexture::copyBufferToImage(VkDevice device, VkCommandPool commandPool,
                                       VkQueue queue, VkBuffer buffer,
                                       u32 width, u32 height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd, buffer, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void VulkanTexture::createImageView(VkDevice device, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view");
    }
}

void VulkanTexture::createSampler(VkDevice device, VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

} // namespace Genesis
