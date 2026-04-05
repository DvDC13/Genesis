#include "renderer/VulkanDescriptors.h"
#include "core/Logger.h"

#include <array>
#include <stdexcept>

namespace Genesis {

void VulkanDescriptors::init(VkDevice device, u32 framesInFlight,
                              const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize uboSize,
                              const std::vector<VkBuffer>& lightBuffers, VkDeviceSize lightUboSize,
                              const std::vector<VkImageView>& textureViews,
                              const std::vector<VkSampler>& textureSamplers,
                              VkImageView shadowMapView, VkSampler shadowMapSampler) {
    m_framesInFlight = framesInFlight;
    m_textureCount   = static_cast<u32>(textureViews.size());
    u32 totalSets    = framesInFlight * m_textureCount;

    // 1. Create descriptor set layout — 4 bindings
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

    // Binding 0: view/proj uniform buffer (vertex stage)
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: combined image sampler (fragment stage) — object texture
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2: lighting uniform buffer (fragment stage)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 3: shadow map sampler (fragment stage)
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // 2. Create descriptor pool — large enough for all sets
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = totalSets * 2; // view/proj + lighting per set
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = totalSets * 2; // texture + shadow map per set

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = totalSets;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // 3. Allocate all descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(totalSets, m_layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_pool;
    allocInfo.descriptorSetCount = totalSets;
    allocInfo.pSetLayouts        = layouts.data();

    m_sets.resize(totalSets);
    if (vkAllocateDescriptorSets(device, &allocInfo, m_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    // 4. Update each descriptor set
    // Layout: sets[textureIndex * framesInFlight + frame]
    for (u32 t = 0; t < m_textureCount; t++) {
        for (u32 f = 0; f < framesInFlight; f++) {
            u32 setIndex = t * framesInFlight + f;

            VkDescriptorBufferInfo vpBufferInfo{};
            vpBufferInfo.buffer = uniformBuffers[f];
            vpBufferInfo.offset = 0;
            vpBufferInfo.range  = uboSize;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = textureViews[t];
            imageInfo.sampler     = textureSamplers[t];

            VkDescriptorBufferInfo lightBufferInfo{};
            lightBufferInfo.buffer = lightBuffers[f];
            lightBufferInfo.offset = 0;
            lightBufferInfo.range  = lightUboSize;

            VkDescriptorImageInfo shadowInfo{};
            shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowInfo.imageView   = shadowMapView;
            shadowInfo.sampler     = shadowMapSampler;

            std::array<VkWriteDescriptorSet, 4> writes{};

            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = m_sets[setIndex];
            writes[0].dstBinding      = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &vpBufferInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = m_sets[setIndex];
            writes[1].dstBinding      = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &imageInfo;

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = m_sets[setIndex];
            writes[2].dstBinding      = 2;
            writes[2].dstArrayElement = 0;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[2].descriptorCount = 1;
            writes[2].pBufferInfo     = &lightBufferInfo;

            writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet          = m_sets[setIndex];
            writes[3].dstBinding      = 3;
            writes[3].dstArrayElement = 0;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].descriptorCount = 1;
            writes[3].pImageInfo      = &shadowInfo;

            vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    Logger::info("Descriptors created ({} sets: {} frames x {} textures, with shadow map)",
                 totalSets, framesInFlight, m_textureCount);
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
