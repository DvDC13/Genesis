#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>

namespace Genesis {

// Holds a buffer + its memory allocation + optional persistent mapping
struct BufferAllocation {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void*          mapped = nullptr;
};

class VulkanBuffer {
public:
    // Find a memory type index that matches the filter and required properties
    static u32 findMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter,
                              VkMemoryPropertyFlags properties);

    // Create a VkBuffer + allocate and bind memory
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory);

    // Copy data between buffers using a one-shot command buffer
    static void copyBuffer(VkDevice device, VkCommandPool commandPool,
                           VkQueue queue, VkBuffer src, VkBuffer dst,
                           VkDeviceSize size);

    // Create a device-local vertex buffer via staging
    static BufferAllocation createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                               VkCommandPool commandPool, VkQueue queue,
                                               const void* data, VkDeviceSize size);

    // Create a device-local index buffer via staging
    static BufferAllocation createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                              VkCommandPool commandPool, VkQueue queue,
                                              const void* data, VkDeviceSize size);

    // Create a host-visible uniform buffer (persistently mapped)
    static BufferAllocation createUniformBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                                VkDeviceSize size);

    // Destroy a buffer allocation
    static void destroy(VkDevice device, BufferAllocation& allocation);
};

} // namespace Genesis
