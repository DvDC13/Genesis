#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace Genesis {

class VulkanPipeline {
public:
    void init(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
              const std::string& vertPath, const std::string& fragPath,
              VkDescriptorSetLayout descriptorSetLayout);
    void shutdown(VkDevice device);

    VkPipeline       getPipeline()       const { return m_pipeline; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

private:
    static std::vector<char> readFile(const std::string& filepath);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

} // namespace Genesis
