#include "renderer/Renderer.h"
#include "renderer/Vertex.h"
#include "platform/Window.h"
#include "core/Logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstring>

namespace Genesis {

void Renderer::init(Window& window) {
    m_window = &window;

    // 1. Create Vulkan instance, debug messenger, and surface
    auto extensions = window.getRequiredVulkanExtensions();
    m_instance.init(extensions, window.getHandle());

    // 2. Pick physical device and create logical device
    m_device.init(m_instance.getInstance(), m_instance.getSurface());

    // 3. Create swapchain, image views, depth buffer, render pass, framebuffers
    auto families = m_device.getQueueFamilies();
    m_swapchain.init(
        m_device.getPhysicalDevice(),
        m_device.getDevice(),
        m_instance.getSurface(),
        window.getWidth(),
        window.getHeight(),
        families.graphicsFamily.value(),
        families.presentFamily.value()
    );

    // 4. Create command pool (needed for staging buffer copies)
    m_commandPool.init(
        m_device.getDevice(),
        families.graphicsFamily.value(),
        MAX_FRAMES_IN_FLIGHT
    );

    // 5. Create cube vertex and index buffers
    createCubeGeometry();

    // 6. Create uniform buffers (one per frame-in-flight)
    createUniformBuffers();

    // 7. Create descriptor sets (binds uniform buffers to shader)
    std::vector<VkBuffer> uboBuffers;
    for (const auto& alloc : m_uniformBuffers) {
        uboBuffers.push_back(alloc.buffer);
    }
    m_descriptors.init(
        m_device.getDevice(),
        MAX_FRAMES_IN_FLIGHT,
        uboBuffers,
        sizeof(UniformBufferObject)
    );

    // 8. Create graphics pipeline (with vertex input + descriptors + depth)
    m_pipeline.init(
        m_device.getDevice(),
        m_swapchain.getRenderPass(),
        m_swapchain.getExtent(),
        "mesh.vert.spv",
        "mesh.frag.spv",
        m_descriptors.getLayout()
    );

    // 9. Create synchronization objects
    m_syncObjects.init(m_device.getDevice(), MAX_FRAMES_IN_FLIGHT, m_swapchain.getImageCount());

    // 10. Capture mouse for FPS camera
    window.setCursorDisabled(true);
    m_lastFrameTime = glfwGetTime();

    Logger::info("Renderer initialized successfully");
}

void Renderer::drawFrame() {
    VkDevice device = m_device.getDevice();

    // Calculate delta time
    f64 currentTime = glfwGetTime();
    f32 deltaTime = static_cast<f32>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;

    // Process keyboard and mouse input
    processInput(deltaTime);

    // Wait for the previous frame using this slot to finish
    VkFence inFlightFence = m_syncObjects.getInFlightFence(m_currentFrame);
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    // Acquire the next swapchain image
    u32 imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device,
        m_swapchain.getSwapchain(),
        UINT64_MAX,
        m_syncObjects.getImageAvailable(m_currentFrame),
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Update the uniform buffer for this frame
    updateUniformBuffer(m_currentFrame);

    vkResetFences(device, 1, &inFlightFence);

    // Record command buffer
    VkCommandBuffer cmd = m_commandPool.getBuffer(m_currentFrame);
    vkResetCommandBuffer(cmd, 0);
    recordCommandBuffer(cmd, imageIndex);

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[]      = { m_syncObjects.getImageAvailable(m_currentFrame) };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;

    VkSemaphore signalSemaphores[] = { m_syncObjects.getRenderFinished(imageIndex) };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapchains[] = { m_swapchain.getSwapchain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains    = swapchains;
    presentInfo.pImageIndices  = &imageIndex;

    result = vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->wasResized()) {
        m_window->resetResizedFlag();
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::shutdown() {
    vkDeviceWaitIdle(m_device.getDevice());

    m_syncObjects.shutdown(m_device.getDevice());
    m_commandPool.shutdown(m_device.getDevice());
    m_pipeline.shutdown(m_device.getDevice());
    m_descriptors.shutdown(m_device.getDevice());

    for (auto& ubo : m_uniformBuffers) {
        VulkanBuffer::destroy(m_device.getDevice(), ubo);
    }
    VulkanBuffer::destroy(m_device.getDevice(), m_indexBuffer);
    VulkanBuffer::destroy(m_device.getDevice(), m_vertexBuffer);

    m_swapchain.shutdown(m_device.getDevice());
    m_device.shutdown();
    m_instance.shutdown();

    Logger::info("Renderer shut down");
}

void Renderer::createCubeGeometry() {
    // 24 vertices: 4 per face with distinct face colors
    // Counter-clockwise winding when viewed from outside
    std::vector<Vertex> vertices = {
        // Front face (red)
        {{ -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }},

        // Back face (green)
        {{  0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }},

        // Top face (blue)
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }},

        // Bottom face (yellow)
        {{ -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f }},
        {{  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 0.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 0.0f }},

        // Right face (cyan)
        {{  0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f }},
        {{  0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 1.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 1.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f }},

        // Left face (magenta)
        {{ -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 1.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 1.0f }},
    };

    // 36 indices: 2 triangles per face, 6 faces
    std::vector<uint16_t> indices;
    for (uint16_t face = 0; face < 6; face++) {
        uint16_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    m_indexCount = static_cast<u32>(indices.size());

    m_vertexBuffer = VulkanBuffer::createVertexBuffer(
        m_device.getDevice(),
        m_device.getPhysicalDevice(),
        m_commandPool.getPool(),
        m_device.getGraphicsQueue(),
        vertices.data(),
        sizeof(Vertex) * vertices.size()
    );

    m_indexBuffer = VulkanBuffer::createIndexBuffer(
        m_device.getDevice(),
        m_device.getPhysicalDevice(),
        m_commandPool.getPool(),
        m_device.getGraphicsQueue(),
        indices.data(),
        sizeof(uint16_t) * indices.size()
    );
}

void Renderer::createUniformBuffers() {
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = VulkanBuffer::createUniformBuffer(
            m_device.getDevice(),
            m_device.getPhysicalDevice(),
            sizeof(UniformBufferObject)
        );
    }
    Logger::info("Uniform buffers created ({} frames)", MAX_FRAMES_IN_FLIGHT);
}

void Renderer::updateUniformBuffer(u32 frameIndex) {
    f32 aspect = static_cast<f32>(m_swapchain.getExtent().width)
               / static_cast<f32>(m_swapchain.getExtent().height);

    UniformBufferObject ubo{};

    // Slowly rotate the cube
    static f32 angle = 0.0f;
    angle += 0.5f; // degrees per frame
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    ubo.view       = m_camera.getViewMatrix();
    ubo.projection = m_camera.getProjectionMatrix(aspect);

    memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));
}

void Renderer::processInput(f32 deltaTime) {
    // ESC to close
    if (m_window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(m_window->getHandle(), true);
    }

    // WASD movement
    if (m_window->isKeyPressed(GLFW_KEY_W))
        m_camera.processKeyboard(CameraDirection::Forward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_S))
        m_camera.processKeyboard(CameraDirection::Backward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_A))
        m_camera.processKeyboard(CameraDirection::Left, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_D))
        m_camera.processKeyboard(CameraDirection::Right, deltaTime);

    // Mouse look
    f32 dx, dy;
    m_window->getMouseDelta(dx, dy);
    if (dx != 0.0f || dy != 0.0f) {
        m_camera.processMouseMovement(dx, dy);
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    // Begin render pass — now with 2 clear values (color + depth)
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass  = m_swapchain.getRenderPass();
    renderPassInfo.framebuffer = m_swapchain.getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapchain.getExtent();

    VkClearValue clearValues[2]{};
    clearValues[0].color        = {{ 0.1f, 0.1f, 0.12f, 1.0f }};
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues    = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());

    // Set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchain.getExtent().width);
    viewport.height   = static_cast<float>(m_swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapchain.getExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = { m_vertexBuffer.buffer };
    VkDeviceSize offsets[]    = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

    // Bind descriptor set (uniform buffer for this frame)
    VkDescriptorSet descriptorSet = m_descriptors.getSet(m_currentFrame);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline.getPipelineLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);

    // Draw the cube
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void Renderer::recreateSwapchain() {
    u32 width = m_window->getWidth();
    u32 height = m_window->getHeight();
    while (width == 0 || height == 0) {
        m_window->pollEvents();
        width = m_window->getWidth();
        height = m_window->getHeight();
    }

    vkDeviceWaitIdle(m_device.getDevice());

    auto families = m_device.getQueueFamilies();
    m_swapchain.recreate(
        m_device.getPhysicalDevice(),
        m_device.getDevice(),
        m_instance.getSurface(),
        width,
        height,
        families.graphicsFamily.value(),
        families.presentFamily.value()
    );
}

} // namespace Genesis
