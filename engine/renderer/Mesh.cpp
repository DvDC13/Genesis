#include "renderer/Mesh.h"
#include "core/Logger.h"

namespace Genesis {

void Mesh::upload(VkDevice device, VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool, VkQueue queue,
                   const std::vector<Vertex>& vertices,
                   const std::vector<u32>& indices) {
    m_indexCount = static_cast<u32>(indices.size());

    m_vertexBuffer = VulkanBuffer::createVertexBuffer(
        device, physicalDevice, commandPool, queue,
        vertices.data(), sizeof(Vertex) * vertices.size()
    );

    m_indexBuffer = VulkanBuffer::createIndexBuffer(
        device, physicalDevice, commandPool, queue,
        indices.data(), sizeof(u32) * indices.size()
    );

    Logger::info("Mesh uploaded ({} vertices, {} indices)", vertices.size(), indices.size());
}

void Mesh::shutdown(VkDevice device) {
    VulkanBuffer::destroy(device, m_indexBuffer);
    VulkanBuffer::destroy(device, m_vertexBuffer);
}

} // namespace Genesis
