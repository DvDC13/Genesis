#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

class VulkanDescriptors {
public:
    // Creates descriptor sets for multiple textures.
    // Total sets = framesInFlight * textureCount.
    // All sets share the same UBO/light buffers, but each texture group has its own sampler.
    void init(VkDevice device, u32 framesInFlight,
              const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize uboSize,
              const std::vector<VkBuffer>& lightBuffers, VkDeviceSize lightUboSize,
              const std::vector<VkImageView>& textureViews,
              const std::vector<VkSampler>& textureSamplers);
    void shutdown(VkDevice device);

    VkDescriptorSetLayout getLayout() const { return m_layout; }

    // Get the descriptor set for a specific frame and texture
    VkDescriptorSet getSet(u32 frame, u32 textureIndex) const {
        return m_sets[textureIndex * m_framesInFlight + frame];
    }

private:
    VkDescriptorSetLayout        m_layout          = VK_NULL_HANDLE;
    VkDescriptorPool             m_pool            = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;
    u32                          m_framesInFlight  = 0;
    u32                          m_textureCount    = 0;
};

} // namespace Genesis
