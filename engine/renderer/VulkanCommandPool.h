#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

class VulkanCommandPool {
public:
    void init(VkDevice device, u32 graphicsFamily, u32 maxFramesInFlight);
    void shutdown(VkDevice device);

    VkCommandPool   getPool() const { return m_commandPool; }
    VkCommandBuffer getBuffer(u32 index) const { return m_commandBuffers[index]; }

private:
    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

} // namespace Genesis
