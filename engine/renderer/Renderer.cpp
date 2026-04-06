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
#include <format>
#include <filesystem>

namespace Genesis {

void Renderer::init(Window& window) {
    m_window = &window;

    // 1. Create Vulkan instance, debug messenger, and surface
    auto extensions = window.getRequiredVulkanExtensions();
    m_instance.init(extensions, window.getHandle());

    // 2. Pick physical device and create logical device
    m_device.init(m_instance.getInstance(), m_instance.getSurface());

    // Store GPU name for stats
    VkPhysicalDeviceProperties gpuProps;
    vkGetPhysicalDeviceProperties(m_device.getPhysicalDevice(), &gpuProps);
    m_gpuName = gpuProps.deviceName;

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

    // 6b. Build VkBuffer lists for sharing with descriptors and skybox
    std::vector<VkBuffer> uboBuffers;
    std::vector<VkBuffer> lightBufs;
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uboBuffers.push_back(m_uniformBuffers[i].buffer);
        lightBufs.push_back(m_lightBuffers[i].buffer);
    }

    // 6c. Create shadow map
    m_shadowMap.init(m_device.getDevice(), m_device.getPhysicalDevice(),
                     m_commandPool.getPool(), m_device.getGraphicsQueue(),
                     MAX_FRAMES_IN_FLIGHT);

    // 6d. Create offscreen viewport framebuffer (scene renders here)
    m_viewportFB.init(m_device.getDevice(), m_device.getPhysicalDevice(),
                      m_swapchain.getExtent().width, m_swapchain.getExtent().height,
                      m_swapchain.getImageFormat());

    // 6e. Create skybox (uses viewport render pass, not swapchain)
    m_skybox.init(m_device.getDevice(), m_device.getPhysicalDevice(),
                  m_commandPool.getPool(), m_device.getGraphicsQueue(),
                  m_viewportFB.getRenderPass(),
                  uboBuffers, sizeof(ViewProjUBO), MAX_FRAMES_IN_FLIGHT);

    // 7. Create descriptor sets (one set per frame per texture, + shadow map)
    std::vector<VkImageView> textureViews;
    std::vector<VkSampler>   textureSamplers;
    for (const auto& tex : m_textures) {
        textureViews.push_back(tex.getImageView());
        textureSamplers.push_back(tex.getSampler());
    }

    m_descriptors.init(
        m_device.getDevice(),
        MAX_FRAMES_IN_FLIGHT,
        uboBuffers, sizeof(ViewProjUBO),
        lightBufs, sizeof(LightUBO),
        textureViews, textureSamplers,
        m_shadowMap.getImageView(), m_shadowMap.getSampler()
    );

    // 8. Create graphics pipeline (uses viewport render pass)
    m_pipeline.init(
        m_device.getDevice(),
        m_viewportFB.getRenderPass(),
        m_viewportFB.getExtent(),
        "phong.vert.spv",
        "phong.frag.spv",
        m_descriptors.getLayout(),
        sizeof(PushConstantData),
        false  // filled
    );

    m_wireframePipeline.init(
        m_device.getDevice(),
        m_viewportFB.getRenderPass(),
        m_viewportFB.getExtent(),
        "phong.vert.spv",
        "phong.frag.spv",
        m_descriptors.getLayout(),
        sizeof(PushConstantData),
        true  // wireframe
    );

    // 9. Create synchronization objects
    m_syncObjects.init(m_device.getDevice(), MAX_FRAMES_IN_FLIGHT, m_swapchain.getImageCount());

    // 10. Initialize ImGui overlay (uses swapchain render pass — ImGui renders to screen)
    m_imgui.init(
        window.getHandle(),
        m_instance.getInstance(),
        m_device.getPhysicalDevice(),
        m_device.getDevice(),
        families.graphicsFamily.value(),
        m_device.getGraphicsQueue(),
        m_swapchain.getRenderPass(),
        m_swapchain.getImageCount()
    );

    // 10b. Register viewport framebuffer as an ImGui texture
    m_viewportFB.createImGuiDescriptor();
    m_imguiState.viewportTexture = m_viewportFB.getImGuiTexture();
    m_imguiState.viewportWidth   = static_cast<f32>(m_viewportFB.getExtent().width);
    m_imguiState.viewportHeight  = static_cast<f32>(m_viewportFB.getExtent().height);

    // 11. Scan for available OBJ models in assets/models/
    scanAvailableModels();

    // 12. Start in UI mode (cursor visible) — press Tab to enter camera mode
    window.setCursorDisabled(false);
    m_cursorCaptured = false;
    m_lastFrameTime = glfwGetTime();
    m_fpsTimer      = m_lastFrameTime;

    Logger::info("Renderer initialized successfully");
}

void Renderer::createScene() {
    // ─── Meshes ───
    // 0: cube
    MeshData cubeData = ModelLoader::createCube();
    m_meshes.emplace_back();
    m_meshes[0].upload(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue(),
        cubeData.vertices, cubeData.indices
    );
    m_meshNames.push_back("Cube");

    // 1: sphere
    MeshData sphereData = ModelLoader::createSphere(0.6f, 36, 18);
    m_meshes.emplace_back();
    m_meshes[1].upload(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue(),
        sphereData.vertices, sphereData.indices
    );
    m_meshNames.push_back("Sphere");

    // 2: torus
    MeshData torusData = ModelLoader::createTorus(0.5f, 0.2f, 36, 24);
    m_meshes.emplace_back();
    m_meshes[2].upload(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue(),
        torusData.vertices, torusData.indices
    );
    m_meshNames.push_back("Torus");

    // ─── Textures ───
    // 0: checkerboard
    m_textures.emplace_back();
    m_textures[0].initCheckerboard(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue()
    );

    // 1: color gradient
    m_textures.emplace_back();
    m_textures[1].initGradient(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue()
    );

    // 2: white (for clean Phong-only look)
    m_textures.emplace_back();
    m_textures[2].initDefault(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue()
    );

    // ─── Scene Objects ───

    // Center: spinning checkerboard cube
    SceneObject centerCube;
    centerCube.name          = "Cube";
    centerCube.position      = { 0.0f, 0.0f, 0.0f };
    centerCube.rotationSpeed = 30.0f;
    centerCube.meshIndex     = 0;
    centerCube.textureIndex  = 0;
    m_objects.push_back(centerCube);

    // Left: rainbow sphere
    SceneObject sphere;
    sphere.name          = "Sphere";
    sphere.position      = { -2.5f, 0.0f, 0.0f };
    sphere.rotationSpeed = 15.0f;
    sphere.meshIndex     = 1;
    sphere.textureIndex  = 1;
    m_objects.push_back(sphere);

    // Right: white torus spinning
    SceneObject torus;
    torus.name           = "Torus";
    torus.position      = { 2.5f, 0.0f, 0.0f };
    torus.rotation       = { 30.0f, 0.0f, 0.0f };
    torus.rotationSpeed = -40.0f;
    torus.meshIndex     = 2;
    torus.textureIndex  = 2;
    m_objects.push_back(torus);

    // Floor: large flat checkerboard cube
    SceneObject floor;
    floor.name           = "Floor";
    floor.position     = { 0.0f, -1.5f, 0.0f };
    floor.scale        = { 12.0f, 0.1f, 12.0f };
    floor.meshIndex    = 0;
    floor.textureIndex = 0;
    m_objects.push_back(floor);

    // Back-left: small gradient cube
    SceneObject backCube;
    backCube.name          = "Small Cube";
    backCube.position      = { -1.5f, 1.0f, -2.0f };
    backCube.scale         = { 0.6f, 0.6f, 0.6f };
    backCube.rotationSpeed = 60.0f;
    backCube.meshIndex     = 0;
    backCube.textureIndex  = 1;
    m_objects.push_back(backCube);

    // Back-right: white sphere
    SceneObject whiteSphere;
    whiteSphere.name         = "White Sphere";
    whiteSphere.position     = { 1.5f, 0.8f, -2.0f };
    whiteSphere.meshIndex    = 1;
    whiteSphere.textureIndex = 2;
    m_objects.push_back(whiteSphere);

    // Far: gradient torus floating
    SceneObject farTorus;
    farTorus.name          = "Far Torus";
    farTorus.position      = { 0.0f, 1.5f, -3.5f };
    farTorus.rotation       = { 45.0f, 0.0f, 0.0f };
    farTorus.rotationSpeed = 25.0f;
    farTorus.meshIndex     = 2;
    farTorus.textureIndex  = 1;
    m_objects.push_back(farTorus);

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

    // FPS counter
    updateFPSCounter();

    // Handle viewport resize BEFORE building ImGui (so the new texture is used)
    if (m_imguiState.viewportResized) {
        m_imguiState.viewportResized = false;
        vkDeviceWaitIdle(m_device.getDevice());
        m_viewportFB.resize(m_device.getDevice(), m_device.getPhysicalDevice(),
                            m_imguiState.newViewportWidth, m_imguiState.newViewportHeight);
        m_imguiState.viewportTexture = m_viewportFB.getImGuiTexture();
    }

    // ─── ImGui frame ───
    m_imgui.newFrame();

    // Feed stats to ImGui
    m_imguiState.fps         = m_lastFPS;
    m_imguiState.gpuName     = m_gpuName.c_str();
    m_imguiState.objectCount = static_cast<u32>(m_objects.size());
    m_imguiState.drawCalls   = static_cast<u32>(m_objects.size());

    u32 totalVerts = 0;
    for (const auto& obj : m_objects) {
        totalVerts += m_meshes[obj.meshIndex].getIndexCount();
    }
    m_imguiState.vertexCount = totalVerts;
    m_imguiState.meshNames   = m_meshNames;

    m_imgui.buildUI(m_imguiState, m_objects);

    // Process any model load request from ImGui
    if (m_imguiState.modelLoadRequested) {
        processModelLoadRequest();
        m_imguiState.modelLoadRequested = false;
    }

    // Process camera preset request
    if (m_imguiState.cameraPresetRequest >= 0) {
        switch (m_imguiState.cameraPresetRequest) {
            case 0: m_camera.setFront();       break;
            case 1: m_camera.setBack();        break;
            case 2: m_camera.setRight();       break;
            case 3: m_camera.setLeft();        break;
            case 4: m_camera.setTop();         break;
            case 5: m_camera.setBottom();      break;
            case 6: m_camera.setPerspective(); break;
        }
        m_imguiState.cameraPresetRequest = -1;
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

    // Update uniform buffers — use ImGui state for lighting
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

    m_viewportFB.shutdown(m_device.getDevice());
    m_imgui.shutdown(m_device.getDevice());
    m_skybox.shutdown(m_device.getDevice());
    m_shadowMap.shutdown(m_device.getDevice());
    m_syncObjects.shutdown(m_device.getDevice());
    m_commandPool.shutdown(m_device.getDevice());
    m_pipeline.shutdown(m_device.getDevice());
    m_wireframePipeline.shutdown(m_device.getDevice());
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
    // Use the viewport framebuffer's aspect ratio (not the swapchain's)
    VkExtent2D vpExtent = m_viewportFB.getExtent();
    f32 aspect = static_cast<f32>(vpExtent.width)
               / static_cast<f32>(vpExtent.height);

    // Compute light space matrix for shadow mapping
    // The light is directional — position it far along the light direction
    glm::vec3 lightDir = glm::normalize(m_imguiState.lightDir);
    glm::vec3 lightPos = lightDir * 15.0f; // Far enough to cover the scene
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProj = glm::ortho(-12.0f, 12.0f, -12.0f, 12.0f, 0.1f, 40.0f);
    // Vulkan Y-axis flip for the light projection too
    lightProj[1][1] *= -1.0f;
    glm::mat4 lightSpaceMatrix = lightProj * lightView;

    ViewProjUBO ubo{};
    ubo.view             = m_camera.getViewMatrix();
    ubo.projection       = m_camera.getProjectionMatrix(aspect);
    ubo.lightSpaceMatrix = lightSpaceMatrix;
    memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));

    // Pass camera matrices to UI state (for gizmos)
    m_imguiState.viewMatrix       = ubo.view;
    m_imguiState.projectionMatrix = ubo.projection;

    // Update shadow map's light VP buffer
    m_shadowMap.updateLightMatrix(frameIndex, lightSpaceMatrix);

    // Use ImGui-controlled lighting values
    LightUBO light{};
    light.lightDir        = m_imguiState.lightDir;
    light.lightColor      = m_imguiState.lightColor;
    light.viewPos         = m_camera.getPosition();
    light.ambientStrength = m_imguiState.ambientStrength;
    memcpy(m_lightBuffers[frameIndex].mapped, &light, sizeof(light));
}

void Renderer::processInput(f32 deltaTime) {
    if (m_window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(m_window->getHandle(), true);
    }

    // Tab toggles cursor capture (camera vs UI mode)
    static bool tabWasPressed = false;
    bool tabPressed = m_window->isKeyPressed(GLFW_KEY_TAB);
    if (tabPressed && !tabWasPressed) {
        m_cursorCaptured = !m_cursorCaptured;
        m_window->setCursorDisabled(m_cursorCaptured);
    }
    tabWasPressed = tabPressed;

    // Camera keyboard movement only when cursor is captured (Tab mode)
    // In editor mode, W/E/R are gizmo shortcuts instead
    if (!m_cursorCaptured) return;

    if (m_window->isKeyPressed(GLFW_KEY_W))
        m_camera.processKeyboard(CameraDirection::Forward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_S))
        m_camera.processKeyboard(CameraDirection::Backward, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_A))
        m_camera.processKeyboard(CameraDirection::Left, deltaTime);
    if (m_window->isKeyPressed(GLFW_KEY_D))
        m_camera.processKeyboard(CameraDirection::Right, deltaTime);

    // Mouse look only when cursor is captured (Tab mode)
    if (m_cursorCaptured) {
        f32 dx, dy;
        m_window->getMouseDelta(dx, dy);
        if (dx != 0.0f || dy != 0.0f) {
            m_camera.processMouseMovement(dx, dy);
        }
    }
}

void Renderer::updateFPSCounter() {
    m_frameCount++;
    f64 currentTime = glfwGetTime();
    f64 elapsed = currentTime - m_fpsTimer;

    if (elapsed >= 0.5) {
        m_lastFPS    = static_cast<f32>(m_frameCount / elapsed);
        m_frameCount = 0;
        m_fpsTimer   = currentTime;

        std::string title = std::format("Genesis Editor | {:.0f} FPS | {} objects",
                                        m_lastFPS, m_objects.size());
        m_window->setTitle(title);
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    // ═══════════════════════════════════════════════
    // PASS 1: Shadow map — render scene depth from light's perspective
    // ═══════════════════════════════════════════════
    m_shadowMap.beginShadowPass(cmd, m_currentFrame);

    for (const auto& obj : m_objects) {
        glm::mat4 model = obj.getModelMatrix();
        vkCmdPushConstants(cmd, m_shadowMap.getPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(glm::mat4), &model);

        const Mesh& mesh = m_meshes[obj.meshIndex];
        VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };
        VkDeviceSize offsets[]    = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.getIndexCount(), 1, 0, 0, 0);
    }

    m_shadowMap.endShadowPass(cmd);

    // ═══════════════════════════════════════════════
    // PASS 2: Offscreen scene — render 3D world to viewport framebuffer
    // ═══════════════════════════════════════════════
    {
        VkExtent2D vpExtent = m_viewportFB.getExtent();

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass  = m_viewportFB.getRenderPass();
        rpInfo.framebuffer = m_viewportFB.getFramebuffer();
        rpInfo.renderArea.offset = { 0, 0 };
        rpInfo.renderArea.extent = vpExtent;

        VkClearValue clearValues[2]{};
        clearValues[0].color        = {{ 0.02f, 0.02f, 0.04f, 1.0f }};
        clearValues[1].depthStencil = { 1.0f, 0 };
        rpInfo.clearValueCount = 2;
        rpInfo.pClearValues    = clearValues;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set viewport and scissor to offscreen size
        VkViewport viewport{};
        viewport.width    = static_cast<float>(vpExtent.width);
        viewport.height   = static_cast<float>(vpExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = vpExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Draw skybox first (behind everything)
        m_skybox.render(cmd, m_currentFrame);

        // Bind scene pipeline and draw each scene object with its own texture
        VkPipeline activePipeline = m_imguiState.wireframeMode
            ? m_wireframePipeline.getPipeline()
            : m_pipeline.getPipeline();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
        for (const auto& obj : m_objects) {
            VkDescriptorSet descriptorSet = m_descriptors.getSet(m_currentFrame, obj.textureIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipeline.getPipelineLayout(), 0, 1,
                                    &descriptorSet, 0, nullptr);

            PushConstantData push{};
            push.model     = obj.getModelMatrix();
            push.albedo    = obj.albedo;
            push.metallic  = obj.metallic;
            push.roughness = obj.roughness;
            push.ao        = obj.ao;
            vkCmdPushConstants(cmd, m_pipeline.getPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushConstantData), &push);

            const Mesh& mesh = m_meshes[obj.meshIndex];
            VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };
            VkDeviceSize offsets[]    = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, mesh.getIndexCount(), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

    // ═══════════════════════════════════════════════
    // PASS 3: Swapchain — render ImGui editor (viewport shown as texture)
    // ═══════════════════════════════════════════════
    {
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass  = m_swapchain.getRenderPass();
        rpInfo.framebuffer = m_swapchain.getFramebuffer(imageIndex);
        rpInfo.renderArea.offset = { 0, 0 };
        rpInfo.renderArea.extent = m_swapchain.getExtent();

        VkClearValue clearValues[2]{};
        clearValues[0].color        = {{ 0.10f, 0.10f, 0.10f, 1.0f }}; // Dark gray editor bg
        clearValues[1].depthStencil = { 1.0f, 0 };
        rpInfo.clearValueCount = 2;
        rpInfo.pClearValues    = clearValues;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // ImGui renders the entire editor UI including the viewport image
        m_imgui.render(cmd);

        vkCmdEndRenderPass(cmd);
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void Renderer::scanAvailableModels() {
    namespace fs = std::filesystem;

    // Look for assets/models/ relative to the executable, or a known project path
    std::vector<std::string> searchPaths = {
        "assets/models",
        "../assets/models",
        "../../assets/models",
        "../../../assets/models",
        "../../../../assets/models",
    };

    m_imguiState.availableModels.clear();

    for (const auto& dir : searchPaths) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".obj") {
                m_imguiState.availableModels.push_back(entry.path().string());
            }
        }

        if (!m_imguiState.availableModels.empty()) {
            Logger::info("Found {} OBJ files in {}", m_imguiState.availableModels.size(), dir);
            break;
        }
    }

    if (m_imguiState.availableModels.empty()) {
        Logger::warn("No .obj files found in assets/models/");
    }
}

void Renderer::processModelLoadRequest() {
    const std::string& path = m_imguiState.pendingModelPath;
    f32 scale = m_imguiState.modelScale;
    u32 texIdx = static_cast<u32>(m_imguiState.modelTexture);
    if (texIdx >= m_textures.size()) texIdx = 0;

    // Wait for GPU to finish before uploading new buffers
    vkDeviceWaitIdle(m_device.getDevice());

    MeshData data;
    std::string meshName;

    try {
        if (path == "__procedural_cube__") {
            data = ModelLoader::createCube();
            meshName = "Cube";
        } else if (path == "__procedural_sphere__") {
            data = ModelLoader::createSphere(0.6f, 36, 18);
            meshName = "Sphere";
        } else if (path == "__procedural_torus__") {
            data = ModelLoader::createTorus(0.5f, 0.2f, 36, 24);
            meshName = "Torus";
        } else {
            data = ModelLoader::loadOBJ(path);
            // Extract filename without extension for the name
            namespace fs = std::filesystem;
            meshName = fs::path(path).stem().string();
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to load model: {}", e.what());
        return;
    }

    if (data.vertices.empty()) {
        Logger::warn("Model has no vertices: {}", path);
        return;
    }

    // Upload mesh
    u32 meshIdx = static_cast<u32>(m_meshes.size());
    m_meshes.emplace_back();
    m_meshes.back().upload(
        m_device.getDevice(), m_device.getPhysicalDevice(),
        m_commandPool.getPool(), m_device.getGraphicsQueue(),
        data.vertices, data.indices
    );
    m_meshNames.push_back(meshName);

    // Create a new scene object at the origin
    SceneObject obj;
    obj.name         = meshName;
    obj.position     = { 0.0f, 0.0f, 0.0f };
    obj.scale        = glm::vec3(scale);
    obj.meshIndex    = meshIdx;
    obj.textureIndex = texIdx;
    obj.rotationSpeed = 15.0f; // gentle spin so you can see all sides
    m_objects.push_back(obj);

    Logger::info("Added {} to scene (mesh {}, {} verts, {} tris, scale {:.1f})",
                 meshName, meshIdx, data.vertices.size(), data.indices.size() / 3, scale);
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
