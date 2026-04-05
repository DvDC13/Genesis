#pragma once

#include "renderer/VulkanBuffer.h"
#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

class Skybox {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkCommandPool commandPool, VkQueue queue,
              VkRenderPass renderPass,
              const std::vector<VkBuffer>& viewProjBuffers,
              VkDeviceSize viewProjSize, u32 framesInFlight);
    void shutdown(VkDevice device);

    void render(VkCommandBuffer cmd, u32 currentFrame);

private:
    void createCubemap(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue queue);
    void createMesh(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue queue);
    void createDescriptors(VkDevice device, u32 framesInFlight,
                           const std::vector<VkBuffer>& viewProjBuffers,
                           VkDeviceSize viewProjSize);
    void createPipeline(VkDevice device, VkRenderPass renderPass);

    // Cubemap
    VkImage        m_cubemapImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_cubemapMemory     = VK_NULL_HANDLE;
    VkImageView    m_cubemapView       = VK_NULL_HANDLE;
    VkSampler      m_cubemapSampler    = VK_NULL_HANDLE;

    // Mesh (simple cube, positions only)
    BufferAllocation m_vertexBuffer;
    BufferAllocation m_indexBuffer;
    u32              m_indexCount = 0;

    // Pipeline
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout        m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool             m_descriptorPool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
};

} // namespace Genesis
