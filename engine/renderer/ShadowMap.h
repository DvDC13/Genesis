#pragma once

#include "core/Types.h"
#include "renderer/VulkanBuffer.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace Genesis {

class ShadowMap {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkCommandPool commandPool, VkQueue queue,
              u32 framesInFlight);
    void shutdown(VkDevice device);

    // Call before drawing shadow casters
    void beginShadowPass(VkCommandBuffer cmd, u32 currentFrame);

    // Call after drawing shadow casters
    void endShadowPass(VkCommandBuffer cmd);

    // Update the light's view-projection matrix for this frame
    void updateLightMatrix(u32 frameIndex, const glm::mat4& lightSpaceMatrix);

    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    VkPipeline       getPipeline()       const { return m_pipeline; }
    VkImageView      getImageView()      const { return m_depthImageView; }
    VkSampler        getSampler()        const { return m_sampler; }

    static constexpr u32 SHADOW_MAP_SIZE = 2048;

private:
    void createDepthResources(VkDevice device, VkPhysicalDevice physicalDevice);
    void createRenderPass(VkDevice device);
    void createFramebuffer(VkDevice device);
    void createDescriptors(VkDevice device, VkPhysicalDevice physicalDevice, u32 framesInFlight);
    void createPipeline(VkDevice device);

    // Depth image
    VkImage        m_depthImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory     = VK_NULL_HANDLE;
    VkImageView    m_depthImageView  = VK_NULL_HANDLE;
    VkSampler      m_sampler         = VK_NULL_HANDLE;

    // Render pass and framebuffer for shadow pass
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    // Pipeline (depth-only, uses existing Vertex format)
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // Descriptors — one UBO per frame (light VP matrix)
    VkDescriptorSetLayout        m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool             m_descriptorPool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    // UBO buffers for light's view-projection matrix
    std::vector<BufferAllocation> m_lightVPBuffers;

    u32 m_framesInFlight = 0;
};

} // namespace Genesis
