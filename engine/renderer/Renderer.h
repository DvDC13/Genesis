#pragma once

#include "renderer/VulkanInstance.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanPipeline.h"
#include "renderer/VulkanCommandPool.h"
#include "renderer/VulkanSyncObjects.h"
#include "renderer/VulkanBuffer.h"
#include "renderer/VulkanDescriptors.h"
#include "renderer/Camera.h"
#include "core/Types.h"

#include <glm/glm.hpp>
#include <vector>

namespace Genesis {

class Window;

// Uniform buffer object — sent to the vertex shader every frame
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

class Renderer {
public:
    void init(Window& window);
    void drawFrame();
    void shutdown();

private:
    void createCubeGeometry();
    void createUniformBuffers();
    void updateUniformBuffer(u32 frameIndex);
    void processInput(f32 deltaTime);
    void recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex);
    void recreateSwapchain();

    Window* m_window = nullptr;

    VulkanInstance    m_instance;
    VulkanDevice      m_device;
    VulkanSwapchain   m_swapchain;
    VulkanPipeline    m_pipeline;
    VulkanCommandPool m_commandPool;
    VulkanSyncObjects m_syncObjects;
    VulkanDescriptors m_descriptors;
    Camera            m_camera;

    // Geometry
    BufferAllocation m_vertexBuffer;
    BufferAllocation m_indexBuffer;
    u32              m_indexCount = 0;

    // Uniform buffers (one per frame-in-flight)
    std::vector<BufferAllocation> m_uniformBuffers;

    // Timing
    f64 m_lastFrameTime = 0.0;

    u32 m_currentFrame = 0;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Genesis
