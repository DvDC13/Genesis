#pragma once

#include "renderer/Vertex.h"
#include "renderer/VulkanBuffer.h"
#include "core/Types.h"

#include <vector>

namespace Genesis {

class Mesh {
public:
    void upload(VkDevice device, VkPhysicalDevice physicalDevice,
                VkCommandPool commandPool, VkQueue queue,
                const std::vector<Vertex>& vertices,
                const std::vector<u32>& indices);
    void shutdown(VkDevice device);

    VkBuffer getVertexBuffer() const { return m_vertexBuffer.buffer; }
    VkBuffer getIndexBuffer()  const { return m_indexBuffer.buffer; }
    u32      getIndexCount()   const { return m_indexCount; }

private:
    BufferAllocation m_vertexBuffer;
    BufferAllocation m_indexBuffer;
    u32              m_indexCount = 0;
};

} // namespace Genesis
