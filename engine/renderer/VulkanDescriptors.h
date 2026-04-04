#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

class VulkanDescriptors {
public:
    void init(VkDevice device, u32 framesInFlight,
              const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize bufferSize);
    void shutdown(VkDevice device);

    VkDescriptorSetLayout getLayout() const { return m_layout; }
    VkDescriptorSet       getSet(u32 frame) const { return m_sets[frame]; }

private:
    VkDescriptorSetLayout        m_layout = VK_NULL_HANDLE;
    VkDescriptorPool             m_pool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;
};

} // namespace Genesis
