#include "renderer/VulkanDescriptors.h"
#include "core/Logger.h"

#include <array>
#include <stdexcept>

namespace Genesis {

void VulkanDescriptors::init(VkDevice device, u32 framesInFlight,
                              const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize uboSize,
                              const std::vector<VkBuffer>& lightBuffers, VkDeviceSize lightUboSize,
                              VkImageView textureImageView, VkSampler textureSampler) {
    // 1. Create descriptor set layout — 3 bindings
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: MVP uniform buffer (vertex stage)
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: combined image sampler (fragment stage)
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2: lighting uniform buffer (fragment stage)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // 2. Create descriptor pool — need UBO descriptors and sampler descriptors
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight * 2; // MVP + lighting UBO per frame
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
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

    // 4. Update each descriptor set
    for (u32 i = 0; i < framesInFlight; i++) {
        // Binding 0: MVP UBO
        VkDescriptorBufferInfo mvpBufferInfo{};
        mvpBufferInfo.buffer = uniformBuffers[i];
        mvpBufferInfo.offset = 0;
        mvpBufferInfo.range  = uboSize;

        // Binding 1: texture sampler
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = textureImageView;
        imageInfo.sampler     = textureSampler;

        // Binding 2: lighting UBO
        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = lightBuffers[i];
        lightBufferInfo.offset = 0;
        lightBufferInfo.range  = lightUboSize;

        std::array<VkWriteDescriptorSet, 3> writes{};

        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = m_sets[i];
        writes[0].dstBinding      = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &mvpBufferInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = m_sets[i];
        writes[1].dstBinding      = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &imageInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = m_sets[i];
        writes[2].dstBinding      = 2;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo     = &lightBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    Logger::info("Descriptors created ({} sets, 3 bindings each)", framesInFlight);
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
