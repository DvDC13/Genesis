#include "renderer/VulkanDescriptors.h"
#include "core/Logger.h"

#include <stdexcept>

namespace Genesis {

void VulkanDescriptors::init(VkDevice device, u32 framesInFlight,
                              const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize bufferSize) {
    // 1. Create descriptor set layout — one UBO binding at binding 0, vertex stage
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding            = 0;
    uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount    = 1;
    uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // 2. Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = framesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // 3. Allocate descriptor sets (one per frame-in-flight)
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_pool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts        = layouts.data();

    m_sets.resize(framesInFlight);
    if (vkAllocateDescriptorSets(device, &allocInfo, m_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    // 4. Update each descriptor set to point to its uniform buffer
    for (u32 i = 0; i < framesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = bufferSize;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = m_sets[i];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    Logger::info("Descriptors created ({} sets)", framesInFlight);
}

void VulkanDescriptors::shutdown(VkDevice device) {
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_pool, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
    }
    Logger::info("Descriptors destroyed");
}

} // namespace Genesis
