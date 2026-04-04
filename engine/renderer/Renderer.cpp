#include "renderer/Renderer.h"
#include "renderer/Vertex.h"
#include "renderer/ModelLoader.h"
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

    // 4. Create command pool
    m_commandPool.init(
        m_device.getDevice(),
        families.graphicsFamily.value(),
        MAX_FRAMES_IN_FLIGHT
    );

    // 5. Create scene (meshes, textures, objects)
    createScene();

    // 6. Create uniform buffers (view/proj + lighting)
    createUniformBuffers();

    // 7. Create descriptor sets (view/proj UBO + texture sampler + lighting UBO)
    //    Uses the first texture for descriptors (all objects share descriptors for now)
    std::vector<VkBuffer> uboBuffers;
    std::vector<VkBuffer> lightBufs;
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uboBuffers.push_back(m_uniformBuffers[i].buffer);
        lightBufs.push_back(m_lightBuffers[i].buffer);
    }
    m_descriptors.init(
        m_device.getDevice(),
        MAX_FRAMES_IN_FLIGHT,
        uboBuffers, sizeof(ViewProjUBO),
        lightBufs, sizeof(LightUBO),
        m_textures[0].getImageView(),
        m_textures[0].getSampler()
    );

    // 8. Create graphics pipeline (with push constants for per-object model matrix)
    m_pipeline.init(
        m_device.getDevice(),
        m_swapchain.getRenderPass(),
        m_swapchain.getExtent(),
        "phong.vert.spv",
        "phong.frag.spv",
        m_descriptors.getLayout(),
        sizeof(PushConstantData)
    );

    // 9. Create synchronization objects
    m_syncObjects.init(m_device.getDevice(), MAX_FRAMES_IN_FLIGHT, m_swapchain.getImageCount());

    // 10. Capture mouse for FPS camera
    window.setCursorDisabled(true);
    m_lastFrameTime = glfwGetTime();

    Logger::info("Renderer initialized successfully");
}

void Renderer::createScene() {
    // --- Meshes ---
    // Mesh 0: cube
    MeshData cubeData = ModelLoader::createCube();
    m_meshes.emplace_back();
    m_meshes[0].upload(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue(),
        cubeData.vertices, cubeData.indices
    );

    // --- Textures ---
    // Texture 0: checkerboard
    m_textures.emplace_back();
    m_textures[0].initCheckerboard(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue()
    );

    // --- Scene Objects ---
    // Center cube (spinning)
    SceneObject center;
    center.position      = { 0.0f, 0.0f, 0.0f };
    center.rotationSpeed = 30.0f; // 30 degrees/sec
    center.meshIndex     = 0;
    center.textureIndex  = 0;
    m_objects.push_back(center);

    // Left cube (static, tilted)
    SceneObject left;
    left.position = { -2.5f, 0.0f, 0.0f };
    left.rotation = { 20.0f, 45.0f, 0.0f };
    left.meshIndex    = 0;
    left.textureIndex = 0;
    m_objects.push_back(left);

    // Right cube (spinning other direction)
    SceneObject right;
    right.position      = { 2.5f, 0.0f, 0.0f };
    right.rotationSpeed = -45.0f;
    right.meshIndex     = 0;
    right.textureIndex  = 0;
    m_objects.push_back(right);

    // Floor (large flat cube)
    SceneObject floor;
    floor.position = { 0.0f, -1.5f, 0.0f };
    floor.scale    = { 10.0f, 0.1f, 10.0f };
    floor.meshIndex    = 0;
    floor.textureIndex = 0;
    m_objects.push_back(floor);

    // Small elevated cube
    SceneObject elevated;
    elevated.position      = { 0.0f, 2.0f, -2.0f };
    elevated.scale         = { 0.5f, 0.5f, 0.5f };
    elevated.rotationSpeed = 90.0f;
    elevated.meshIndex     = 0;
    elevated.textureIndex  = 0;
    m_objects.push_back(elevated);

    Logger::info("Scene created ({} objects, {} meshes, {} textures)",
                 m_objects.size(), m_meshes.size(), m_textures.size());
}

void Renderer::drawFrame() {
    VkDevice device = m_device.getDevice();

    // Calculate delta time
    f64 currentTime = glfwGetTime();
    f32 deltaTime = static_cast<f32>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;

    // Process keyboard and mouse input
    processInput(deltaTime);

    // Update scene objects
    for (auto& obj : m_objects) {
        obj.update(deltaTime);
    }

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

    // Update uniform buffers for this frame
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
    for (auto& lb : m_lightBuffers) {
        VulkanBuffer::destroy(m_device.getDevice(), lb);
    }

    for (auto& tex : m_textures) {
        tex.shutdown(m_device.getDevice());
    }
    for (auto& mesh : m_meshes) {
        mesh.shutdown(m_device.getDevice());
    }

    m_swapchain.shutdown(m_device.getDevice());
    m_device.shutdown();
    m_instance.shutdown();

    Logger::info("Renderer shut down");
}

void Renderer::createUniformBuffers() {
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_lightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = VulkanBuffer::createUniformBuffer(
            m_device.getDevice(),
            m_device.getPhysicalDevice(),
            sizeof(ViewProjUBO)
        );
        m_lightBuffers[i] = VulkanBuffer::createUniformBuffer(
            m_device.getDevice(),
            m_device.getPhysicalDevice(),
            sizeof(LightUBO)
        );
    }
    Logger::info("Uniform buffers created ({} frames, view/proj + lighting)", MAX_FRAMES_IN_FLIGHT);
}

void Renderer::updateUniformBuffer(u32 frameIndex) {
    f32 aspect = static_cast<f32>(m_swapchain.getExtent().width)
               / static_cast<f32>(m_swapchain.getExtent().height);

    // View/Projection UBO (shared by all objects — model matrix is per-object via push constant)
    ViewProjUBO ubo{};
    ubo.view       = m_camera.getViewMatrix();
    ubo.projection = m_camera.getProjectionMatrix(aspect);
    memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));

    // Lighting UBO
    LightUBO light{};
    light.lightDir        = glm::normalize(glm::vec3(1.0f, 1.0f, 0.5f));
    light.lightColor      = glm::vec3(1.0f, 1.0f, 1.0f);
    light.viewPos         = m_camera.getPosition();
    light.ambientStrength = 0.15f;
    memcpy(m_lightBuffers[frameIndex].mapped, &light, sizeof(light));
}

void Renderer::processInput(f32 deltaTime) {
    if (m_window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(m_window->getHandle(), true);
    }

    if (m_window->isKeyPressed(GLFW_KEY_W))
        m_camera.processKeyboard(CameraDirection::Forward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_S))
        m_camera.processKeyboard(CameraDirection::Backward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_A))
        m_camera.processKeyboard(CameraDirection::Left, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_D))
        m_camera.processKeyboard(CameraDirection::Right, deltaTime);

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

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass  = m_swapchain.getRenderPass();
    renderPassInfo.framebuffer = m_swapchain.getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapchain.getExtent();

    VkClearValue clearValues[2]{};
    clearValues[0].color        = {{ 0.05f, 0.05f, 0.07f, 1.0f }};
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues    = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());

    // Set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.width    = static_cast<float>(m_swapchain.getExtent().width);
    viewport.height   = static_cast<float>(m_swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_swapchain.getExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind descriptor set (view/proj + texture + lighting for this frame)
    VkDescriptorSet descriptorSet = m_descriptors.getSet(m_currentFrame);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline.getPipelineLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);

    // Draw each scene object
    for (const auto& obj : m_objects) {
        // Push the model matrix for this object
        PushConstantData push{};
        push.model = obj.getModelMatrix();
        vkCmdPushConstants(cmd, m_pipeline.getPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstantData), &push);

        // Bind mesh
        const Mesh& mesh = m_meshes[obj.meshIndex];
        VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };
        VkDeviceSize offsets[]    = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Draw
        vkCmdDrawIndexed(cmd, mesh.getIndexCount(), 1, 0, 0, 0);
    }

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
