#pragma once

#include "core/Types.h"
#include "renderer/SceneObject.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct GLFWwindow;

namespace Genesis {

struct ImGuiState {
    // Lighting controls
    glm::vec3 lightDir        = glm::normalize(glm::vec3(1.0f, 1.0f, 0.5f));
    glm::vec3 lightColor      = glm::vec3(1.0f, 1.0f, 1.0f);
    f32       ambientStrength  = 0.15f;

    // Scene info
    f32 fps            = 0.0f;
    u32 objectCount    = 0;
    u32 vertexCount    = 0;
    u32 drawCalls      = 0;
    const char* gpuName = "";

    // Selection (supports multi-select with Ctrl/Shift)
    std::vector<bool> selected;  // parallel to objects vector
    i32 lastClickedIndex = -1;   // for Shift-range selection

    // Helpers
    i32 getFirstSelected() const {
        for (i32 i = 0; i < static_cast<i32>(selected.size()); i++)
            if (selected[i]) return i;
        return -1;
    }
    i32 getSelectedCount() const {
        i32 count = 0;
        for (bool s : selected) if (s) count++;
        return count;
    }
    void clearSelection() {
        std::fill(selected.begin(), selected.end(), false);
        lastClickedIndex = -1;
    }
    void ensureSelectionSize(i32 objectCount) {
        selected.resize(static_cast<size_t>(objectCount), false);
    }

    // Model loading request (set by UI, consumed by Renderer)
    std::string pendingModelPath;
    bool        modelLoadRequested = false;
    f32         modelScale         = 1.0f;
    i32         modelTexture       = 0;  // 0=checker, 1=gradient, 2=white

    // Available OBJ files (populated by Renderer)
    std::vector<std::string> availableModels;
    i32 selectedModelIndex = 0;

    // Mesh names for the object list (index -> name)
    std::vector<std::string> meshNames;

    // Viewport info
    VkDescriptorSet viewportTexture = VK_NULL_HANDLE;
    f32 viewportWidth  = 0.0f;
    f32 viewportHeight = 0.0f;
    bool viewportHovered = false; // true when mouse is over the 3D viewport
    bool viewportResized = false; // true when viewport panel changed size
    u32  newViewportWidth  = 0;
    u32  newViewportHeight = 0;
};

class ImGuiOverlay {
public:
    void init(GLFWwindow* window, VkInstance instance,
              VkPhysicalDevice physicalDevice, VkDevice device,
              u32 graphicsFamily, VkQueue graphicsQueue,
              VkRenderPass renderPass, u32 imageCount);
    void shutdown(VkDevice device);

    void newFrame();
    void buildUI(ImGuiState& state, std::vector<SceneObject>& objects);
    void render(VkCommandBuffer cmd);

private:
    void buildDockspace();
    void buildMenuBar(ImGuiState& state);
    void buildViewport(ImGuiState& state);
    void buildSceneHierarchy(ImGuiState& state, std::vector<SceneObject>& objects);
    void buildProperties(ImGuiState& state, std::vector<SceneObject>& objects);
    void buildStats(ImGuiState& state);
    void buildModelLoader(ImGuiState& state);

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_firstFrame = true;
    i32  m_renamingIndex = -1;     // which object is being renamed (-1 = none)
    char m_renameBuffer[128] = {}; // text input buffer for renaming
};

} // namespace Genesis
