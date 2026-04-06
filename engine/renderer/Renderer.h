#pragma once

#include "renderer/VulkanInstance.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanPipeline.h"
#include "renderer/VulkanCommandPool.h"
#include "renderer/VulkanSyncObjects.h"
#include "renderer/VulkanBuffer.h"
#include "renderer/VulkanDescriptors.h"
#include "renderer/VulkanTexture.h"
#include "renderer/Mesh.h"
#include "renderer/SceneObject.h"
#include "renderer/Camera.h"
#include "renderer/Skybox.h"
#include "renderer/ShadowMap.h"
#include "renderer/ViewportFramebuffer.h"
#include "ui/ImGuiOverlay.h"
#include "core/Types.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Genesis {

class Window;

// Per-frame view/projection data (shared by all objects)
struct ViewProjUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 lightSpaceMatrix;  // Light's VP matrix for shadow mapping
};

// Lighting data — sent to the fragment shader every frame
struct LightUBO {
    glm::vec3 lightDir;
    float     _pad0;
    glm::vec3 lightColor;
    float     _pad1;
    glm::vec3 viewPos;
    float     ambientStrength;
};

class Renderer {
public:
    void init(Window& window);
    void drawFrame();
    void shutdown();

private:
    void createScene();
    void createUniformBuffers();
    void updateUniformBuffer(u32 frameIndex);
    void processInput(f32 deltaTime);
    void recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex);
    void recreateSwapchain();
    void updateFPSCounter();
    void scanAvailableModels();
    void processModelLoadRequest();

    Window* m_window = nullptr;

    VulkanInstance    m_instance;
    VulkanDevice      m_device;
    VulkanSwapchain   m_swapchain;
    VulkanPipeline    m_pipeline;
    VulkanCommandPool m_commandPool;
    VulkanSyncObjects m_syncObjects;
    VulkanDescriptors m_descriptors;
    Camera            m_camera;
    Skybox            m_skybox;
    ShadowMap            m_shadowMap;
    ViewportFramebuffer  m_viewportFB;
    ImGuiOverlay         m_imgui;
    ImGuiState           m_imguiState;

    // Resources (meshes and textures, referenced by SceneObjects by index)
    std::vector<Mesh>           m_meshes;
    std::vector<std::string>    m_meshNames;  // Human-readable name per mesh
    std::vector<VulkanTexture>  m_textures;

    // Scene objects
    std::vector<SceneObject>    m_objects;

    // Uniform buffers (one per frame-in-flight)
    std::vector<BufferAllocation> m_uniformBuffers;
    std::vector<BufferAllocation> m_lightBuffers;

    // Timing
    f64 m_lastFrameTime = 0.0;

    // FPS counter
    u32 m_frameCount    = 0;
    f64 m_fpsTimer      = 0.0;
    f32 m_lastFPS       = 0.0f;

    // Cursor state (Tab to toggle)
    bool m_cursorCaptured = true;

    // GPU name for stats display
    std::string m_gpuName;

    u32 m_currentFrame = 0;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Genesis
