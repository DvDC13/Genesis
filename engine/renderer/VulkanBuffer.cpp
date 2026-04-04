#include "renderer/VulkanBuffer.h"
#include "core/Logger.h"

#include <stdexcept>
#include <cstring>

namespace Genesis {

u32 VulkanBuffer::findMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter,
                                  VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBuffer::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

void VulkanBuffer::copyBuffer(VkDevice device, VkCommandPool commandPool,
                               VkQueue queue, VkBuffer src, VkBuffer dst,
                               VkDeviceSize size) {
    // Allocate a temporary command buffer for the transfer
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

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

BufferAllocation VulkanBuffer::createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                                   VkCommandPool commandPool, VkQueue queue,
                                                   const void* data, VkDeviceSize size) {
    // Create host-visible staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(device, physicalDevice, size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    // Copy vertex data into staging buffer
    void* mapped;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    // Create device-local vertex buffer
    BufferAllocation alloc;
    createBuffer(device, physicalDevice, size,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 alloc.buffer, alloc.memory);

    // Transfer from staging to device-local
    copyBuffer(device, commandPool, queue, stagingBuffer, alloc.buffer, size);

    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    Logger::info("Vertex buffer created ({} bytes)", size);
    return alloc;
}

BufferAllocation VulkanBuffer::createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                                  VkCommandPool commandPool, VkQueue queue,
                                                  const void* data, VkDeviceSize size) {
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(device, physicalDevice, size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    BufferAllocation alloc;
    createBuffer(device, physicalDevice, size,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 alloc.buffer, alloc.memory);

    copyBuffer(device, commandPool, queue, stagingBuffer, alloc.buffer, size);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    Logger::info("Index buffer created ({} bytes)", size);
    return alloc;
}

BufferAllocation VulkanBuffer::createUniformBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                                    VkDeviceSize size) {
    BufferAllocation alloc;
    createBuffer(device, physicalDevice, size,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 alloc.buffer, alloc.memory);

    // Persistently map — stays mapped for the lifetime of the buffer
    vkMapMemory(device, alloc.memory, 0, size, 0, &alloc.mapped);

    return alloc;
}

void VulkanBuffer::destroy(VkDevice device, BufferAllocation& allocation) {
    if (allocation.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, allocation.buffer, nullptr);
    }
    if (allocation.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, allocation.memory, nullptr);
    }
    allocation = {};
}

} // namespace Genesis
