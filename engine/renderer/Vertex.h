#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

namespace Genesis {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};

        // location 0 = position (vec3)
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(Vertex, position);

        // location 1 = color (vec3)
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(Vertex, color);

        return attrs;
    }
};

} // namespace Genesis
