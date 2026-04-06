#include "ui/ImGuiOverlay.h"
#include "core/Logger.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <stdexcept>
#include <format>

namespace Genesis {

void ImGuiOverlay::init(GLFWwindow* window, VkInstance instance,
                         VkPhysicalDevice physicalDevice, VkDevice device,
                         u32 graphicsFamily, VkQueue graphicsQueue,
                         VkRenderPass renderPass, u32 imageCount) {
    // 1. Create a descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 100;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // 2. Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // Don't save/load layout from disk — always use our DockBuilder layout

    // ─── DPI-aware scaling ───
    float xScale = 1.0f, yScale = 1.0f;
    glfwGetWindowContentScale(window, &xScale, &yScale);
    float dpiScale = std::max(xScale, yScale);
    // Ensure a minimum comfortable size (1.25x), bump slightly larger
    dpiScale = std::max(dpiScale, 1.25f) * 1.1f;

    // Load default font at scaled size (default is 13px, too small on HiDPI)
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();
    io.FontGlobalScale = dpiScale;

    // Editor-style dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 2.0f;
    style.FrameRounding    = 2.0f;
    style.GrabRounding     = 2.0f;
    style.TabRounding      = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize  = 0.0f;
    style.WindowPadding    = ImVec2(8.0f, 8.0f);
    style.ItemSpacing      = ImVec2(8.0f, 4.0f);
    style.IndentSpacing    = 16.0f;
    style.ScaleAllSizes(dpiScale);

    // Blender-inspired colors
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.25f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Tab]                = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_TabSelected]        = ImVec4(0.25f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Separator]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // 3. Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 4. Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion     = VK_API_VERSION_1_3;
    initInfo.Instance       = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device         = device;
    initInfo.QueueFamily    = graphicsFamily;
    initInfo.Queue          = graphicsQueue;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount  = 2;
    initInfo.ImageCount     = imageCount;
    initInfo.PipelineInfoMain.RenderPass  = renderPass;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    m_firstFrame = true;

    Logger::info("ImGui overlay initialized (docked editor layout)");
}

void ImGuiOverlay::shutdown(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    }

    Logger::info("ImGui overlay destroyed");
}

void ImGuiOverlay::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::buildUI(ImGuiState& state, std::vector<SceneObject>& objects) {
    ImGuizmo::BeginFrame();
    buildDockspace();
    buildMenuBar(state);
    buildViewport(state);
    buildGizmo(state, objects);
    buildSceneHierarchy(state, objects);
    buildProperties(state, objects);
    buildStats(state);
    buildModelLoader(state);
}

void ImGuiOverlay::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

// ─── Private Panel Builders ───

void ImGuiOverlay::buildDockspace() {
    // Create a fullscreen dockspace that covers the entire window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");

    // Set up default layout on first frame
    if (m_firstFrame) {
        m_firstFrame = false;

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        // Split: left panel (22%) | center+right
        ImGuiID dockLeft, dockCenterRight;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.22f, &dockLeft, &dockCenterRight);

        // Split center+right: center | right panel (25%)
        ImGuiID dockCenter, dockRight;
        ImGui::DockBuilderSplitNode(dockCenterRight, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

        // Split center: viewport | bottom stats bar (small)
        ImGuiID dockViewport, dockBottom;
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.06f, &dockBottom, &dockViewport);

        // Dock windows
        ImGui::DockBuilderDockWindow("Viewport", dockViewport);
        ImGui::DockBuilderDockWindow("Scene", dockLeft);
        ImGui::DockBuilderDockWindow("Model Loader", dockLeft);  // tabbed with Scene
        ImGui::DockBuilderDockWindow("Properties", dockRight);
        ImGui::DockBuilderDockWindow("Lighting", dockRight);     // tabbed with Properties
        ImGui::DockBuilderDockWindow("Stats", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void ImGuiOverlay::buildMenuBar(ImGuiState& state) {
    // The menu bar is inside the dockspace window — we use BeginMainMenuBar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                // Placeholder
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Esc")) {
                // Will be handled by processInput
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) {
                state.pendingModelPath = "__procedural_cube__";
                state.modelLoadRequested = true;
            }
            if (ImGui::MenuItem("Sphere")) {
                state.pendingModelPath = "__procedural_sphere__";
                state.modelLoadRequested = true;
            }
            if (ImGui::MenuItem("Torus")) {
                state.pendingModelPath = "__procedural_torus__";
                state.modelLoadRequested = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load OBJ...")) {
                // Open the model loader panel
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, true);
            ImGui::MenuItem("Model Loader", nullptr, true);
            ImGui::EndMenu();
        }

        // Right-aligned FPS display
        std::string fpsText = std::format("FPS: {:.0f}", state.fps);
        float fpsWidth = ImGui::CalcTextSize(fpsText.c_str()).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - fpsWidth - 16.0f);
        ImGui::TextDisabled("%s", fpsText.c_str());

        ImGui::EndMainMenuBar();
    }
}

void ImGuiOverlay::buildViewport(ImGuiState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    ImGui::PopStyleVar();

    // Get available region for the viewport image
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

    // Clamp to minimum size
    if (viewportSize.x < 64.0f) viewportSize.x = 64.0f;
    if (viewportSize.y < 64.0f) viewportSize.y = 64.0f;

    // Check if viewport was resized (4px tolerance to avoid resize spam)
    u32 newW = static_cast<u32>(viewportSize.x);
    u32 newH = static_cast<u32>(viewportSize.y);
    u32 oldW = static_cast<u32>(state.viewportWidth);
    u32 oldH = static_cast<u32>(state.viewportHeight);
    i32 dw = static_cast<i32>(newW) - static_cast<i32>(oldW);
    i32 dh = static_cast<i32>(newH) - static_cast<i32>(oldH);
    if (dw * dw + dh * dh > 16) { // more than ~4px change
        state.viewportResized   = true;
        state.newViewportWidth  = newW;
        state.newViewportHeight = newH;
        state.viewportWidth     = viewportSize.x;
        state.viewportHeight    = viewportSize.y;
    }

    // Store viewport screen-space position for ImGuizmo
    ImVec2 vpMin = ImGui::GetCursorScreenPos();

    // Display the 3D scene as a texture
    if (state.viewportTexture != VK_NULL_HANDLE) {
        ImTextureID texId = reinterpret_cast<ImTextureID>(state.viewportTexture);
        ImGui::Image(texId, viewportSize);
    }

    // Track if the viewport is hovered (for camera input)
    state.viewportHovered = ImGui::IsItemHovered();

    // Set ImGuizmo drawing rect to match the viewport image
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(vpMin.x, vpMin.y, viewportSize.x, viewportSize.y);

    // ─── Draw ground grid via ImGuizmo ───
    if (state.showGrid) {
        glm::mat4 gizmoProj = state.projectionMatrix;
        gizmoProj[1][1] *= -1.0f; // Un-flip Vulkan Y for ImGuizmo
        glm::mat4 identityMatrix = glm::mat4(1.0f);
        ImGuizmo::DrawGrid(
            glm::value_ptr(state.viewMatrix),
            glm::value_ptr(gizmoProj),
            glm::value_ptr(identityMatrix),
            20.0f  // grid size
        );
    }

    // ─── Viewport overlay toolbar (top-left corner) ───
    {
        ImVec2 toolbarPos(vpMin.x + 8.0f, vpMin.y + 8.0f);
        ImGui::SetCursorScreenPos(toolbarPos);

        // Transparent background for toolbar buttons
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 0.90f));

        ImVec4 onColor(0.25f, 0.50f, 0.75f, 0.85f);

        // Grid toggle
        bool gridOn = state.showGrid;
        if (gridOn) ImGui::PushStyleColor(ImGuiCol_Button, onColor);
        if (ImGui::SmallButton("Grid")) state.showGrid = !state.showGrid;
        if (gridOn) ImGui::PopStyleColor();

        ImGui::SameLine();

        // Wireframe toggle
        bool wireOn = state.wireframeMode;
        if (wireOn) ImGui::PushStyleColor(ImGuiCol_Button, onColor);
        if (ImGui::SmallButton("Wire")) state.wireframeMode = !state.wireframeMode;
        if (wireOn) ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Camera presets
        if (ImGui::SmallButton("Front"))  state.cameraPresetRequest = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("Right"))  state.cameraPresetRequest = 2;
        ImGui::SameLine();
        if (ImGui::SmallButton("Top"))    state.cameraPresetRequest = 4;
        ImGui::SameLine();
        if (ImGui::SmallButton("Persp"))  state.cameraPresetRequest = 6;

        ImGui::PopStyleColor(3); // Button colors
    }

    ImGui::End();
}

void ImGuiOverlay::buildSceneHierarchy(ImGuiState& state, std::vector<SceneObject>& objects) {
    ImGui::Begin("Scene");

    i32 count = static_cast<i32>(objects.size());
    state.ensureSelectionSize(count);

    // Header
    ImGui::Text("Objects: %d", count);
    if (state.getSelectedCount() > 1) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d selected)", state.getSelectedCount());
    }
    ImGui::Separator();

    // Track if we need to break out after modifying the list
    bool listModified = false;

    for (i32 i = 0; i < count && !listModified; i++) {
        ImGui::PushID(i);

        // ─── Renaming mode ───
        if (m_renamingIndex == i) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                objects[i].name = m_renameBuffer;
                m_renamingIndex = -1;
            }
            // Cancel on Escape or click elsewhere
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))) {
                m_renamingIndex = -1;
            }
            // Auto-focus the input on the first frame
            if (ImGui::IsItemDeactivated() == false && ImGui::IsItemFocused() == false) {
                ImGui::SetKeyboardFocusHere(-1);
            }

            ImGui::PopID();
            continue;
        }

        // ─── Normal display ───
        std::string label = std::format("  {}##obj", objects[i].name);

        bool isSelected = state.selected[i];
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            ImGuiIO& io = ImGui::GetIO();

            if (io.KeyCtrl) {
                // Ctrl+click: toggle individual selection
                state.selected[i] = !state.selected[i];
            } else if (io.KeyShift && state.lastClickedIndex >= 0) {
                // Shift+click: range selection
                i32 lo = std::min(state.lastClickedIndex, i);
                i32 hi = std::max(state.lastClickedIndex, i);
                for (i32 j = lo; j <= hi; j++) {
                    state.selected[j] = true;
                }
            } else {
                // Normal click: select only this one
                state.clearSelection();
                state.ensureSelectionSize(count);
                state.selected[i] = true;
            }
            state.lastClickedIndex = i;
        }

        // Double-click to rename
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            m_renamingIndex = i;
            strncpy(m_renameBuffer, objects[i].name.c_str(), sizeof(m_renameBuffer) - 1);
            m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
        }

        // ─── Drag source (reorder) ───
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SCENE_OBJ", &i, sizeof(i32));
            ImGui::Text("Move: %s", objects[i].name.c_str());
            ImGui::EndDragDropSource();
        }

        // ─── Drop target (reorder) ───
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_OBJ")) {
                i32 srcIdx = *static_cast<const i32*>(payload->Data);
                if (srcIdx != i) {
                    // Move the object from srcIdx to i
                    SceneObject moved = std::move(objects[srcIdx]);
                    objects.erase(objects.begin() + srcIdx);
                    i32 insertAt = (srcIdx < i) ? i - 1 : i;
                    objects.insert(objects.begin() + insertAt, std::move(moved));

                    // Update selection
                    state.clearSelection();
                    state.ensureSelectionSize(static_cast<i32>(objects.size()));
                    state.selected[insertAt] = true;
                    state.lastClickedIndex = insertAt;
                    listModified = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // ─── Right-click context menu ───
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename", "F2")) {
                m_renamingIndex = i;
                strncpy(m_renameBuffer, objects[i].name.c_str(), sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                SceneObject copy = objects[i];
                copy.name = objects[i].name + " Copy";
                copy.position += glm::vec3(1.0f, 0.0f, 0.0f);
                objects.insert(objects.begin() + i + 1, copy);
                state.clearSelection();
                state.ensureSelectionSize(static_cast<i32>(objects.size()));
                state.selected[i + 1] = true;
                state.lastClickedIndex = i + 1;
                listModified = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del")) {
                objects.erase(objects.begin() + i);
                state.selected.erase(state.selected.begin() + i);
                if (state.lastClickedIndex >= static_cast<i32>(objects.size()))
                    state.lastClickedIndex = -1;
                listModified = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            if (state.getSelectedCount() > 1 && ImGui::MenuItem("Delete Selected")) {
                for (i32 j = static_cast<i32>(objects.size()) - 1; j >= 0; j--) {
                    if (state.selected[j]) {
                        objects.erase(objects.begin() + j);
                    }
                }
                state.selected.clear();
                state.lastClickedIndex = -1;
                listModified = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // Keyboard shortcuts when Scene panel is focused
    if (ImGui::IsWindowFocused()) {
        // Delete key
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            for (i32 j = static_cast<i32>(objects.size()) - 1; j >= 0; j--) {
                if (j < static_cast<i32>(state.selected.size()) && state.selected[j]) {
                    objects.erase(objects.begin() + j);
                }
            }
            state.selected.clear();
            state.lastClickedIndex = -1;
        }

        // F2 to rename first selected
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            i32 first = state.getFirstSelected();
            if (first >= 0) {
                m_renamingIndex = first;
                strncpy(m_renameBuffer, objects[first].name.c_str(), sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
            }
        }

        // Ctrl+A to select all
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            state.ensureSelectionSize(static_cast<i32>(objects.size()));
            std::fill(state.selected.begin(), state.selected.end(), true);
        }
    }

    ImGui::End();
}

void ImGuiOverlay::buildProperties(ImGuiState& state, std::vector<SceneObject>& objects) {
    ImGui::Begin("Properties");

    i32 sel = state.getFirstSelected();
    if (sel >= 0 && sel < static_cast<i32>(objects.size())) {
        SceneObject& obj = objects[sel];

        // Object header
        const char* meshName = "Unknown";
        if (obj.meshIndex < static_cast<u32>(state.meshNames.size())) {
            meshName = state.meshNames[obj.meshIndex].c_str();
        }
        ImGui::Text("%s  (%s)", obj.name.c_str(), meshName);
        ImGui::Separator();

        // ─── Gizmo mode selector ───
        {
            const char* modes[] = { "Translate (W)", "Rotate (E)", "Scale (R)" };
            ImVec4 activeColor(0.26f, 0.59f, 0.98f, 1.0f);
            ImVec4 normalColor(0.30f, 0.30f, 0.30f, 1.0f);

            for (i32 m = 0; m < 3; m++) {
                if (m > 0) ImGui::SameLine();
                bool isActive = (state.gizmoOperation == m);
                if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
                else          ImGui::PushStyleColor(ImGuiCol_Button, normalColor);

                if (ImGui::Button(modes[m])) {
                    state.gizmoOperation = m;
                }
                ImGui::PopStyleColor();
            }

            ImGui::Checkbox("Snap", &state.gizmoUsingSnap);
            if (state.gizmoUsingSnap) {
                ImGui::SameLine();
                if (state.gizmoOperation == 0)
                    ImGui::DragFloat("##snap", &state.gizmoSnapTranslate, 0.1f, 0.1f, 10.0f, "%.1f");
                else if (state.gizmoOperation == 1)
                    ImGui::DragFloat("##snap", &state.gizmoSnapRotate, 1.0f, 1.0f, 90.0f, "%.0f deg");
                else
                    ImGui::DragFloat("##snap", &state.gizmoSnapScale, 0.05f, 0.05f, 5.0f, "%.2f");
            }
            ImGui::Separator();
        }

        // ─── Transform section ───
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &obj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale",    &obj.scale.x, 0.05f, 0.01f, 20.0f);
            ImGui::DragFloat("Spin Speed", &obj.rotationSpeed, 1.0f);

            if (ImGui::Button("Reset Transform")) {
                obj.position = glm::vec3(0.0f);
                obj.rotation = glm::vec3(0.0f);
                obj.scale    = glm::vec3(1.0f);
                obj.rotationSpeed = 0.0f;
            }
        }

        // ─── Mesh section ───
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Type: %s", meshName);
            ImGui::Text("Index: %u", obj.meshIndex);
        }

        // ─── Material section (PBR) ───
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Texture selector
            const char* texNames[] = { "Checkerboard", "Gradient", "White" };
            i32 texIdx = static_cast<i32>(obj.textureIndex);
            if (texIdx < 3) {
                ImGui::Combo("Texture", reinterpret_cast<int*>(&obj.textureIndex), texNames, 3);
            } else {
                ImGui::Text("Texture: Custom (%u)", obj.textureIndex);
            }

            ImGui::Separator();

            // PBR properties
            ImGui::ColorEdit3("Albedo", &obj.albedo.x);
            ImGui::SliderFloat("Metallic", &obj.metallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness", &obj.roughness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("AO", &obj.ao, 0.0f, 1.0f, "%.2f");

            ImGui::Separator();

            // Material presets
            if (ImGui::Button("Default")) {
                obj.albedo    = glm::vec3(1.0f);
                obj.metallic  = 0.0f;
                obj.roughness = 0.5f;
                obj.ao        = 1.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Plastic")) {
                obj.metallic  = 0.0f;
                obj.roughness = 0.4f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Rough")) {
                obj.metallic  = 0.0f;
                obj.roughness = 0.9f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Metal")) {
                obj.metallic  = 1.0f;
                obj.roughness = 0.2f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mirror")) {
                obj.metallic  = 1.0f;
                obj.roughness = 0.05f;
            }
        }
    } else {
        ImGui::TextDisabled("No object selected");
        ImGui::TextDisabled("Select an object in the Scene panel");
    }

    ImGui::End();

    // ─── Lighting panel (separate, tabbed with Properties) ───
    ImGui::Begin("Lighting");

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat3("Direction", &state.lightDir.x, -1.0f, 1.0f);
        ImGui::ColorEdit3("Color", &state.lightColor.x);
        ImGui::SliderFloat("Ambient", &state.ambientStrength, 0.0f, 1.0f);

        // Normalize light direction after editing
        float len = glm::length(state.lightDir);
        if (len > 0.001f) {
            state.lightDir /= len;
        }
    }

    ImGui::End();
}

void ImGuiOverlay::buildStats(ImGuiState& state) {
    ImGui::Begin("Stats");

    ImGui::Text("GPU: %s", state.gpuName);
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("Objects: %u", state.objectCount);
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("Draw Calls: %u", state.drawCalls);
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("Vertices: %u", state.vertexCount);
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("FPS: %.0f", state.fps);

    ImGui::End();
}

void ImGuiOverlay::buildModelLoader(ImGuiState& state) {
    ImGui::Begin("Model Loader");

    // Model selector from available files
    if (!state.availableModels.empty()) {
        ImGui::Text("Available Models:");
        ImGui::Separator();
        for (i32 i = 0; i < static_cast<i32>(state.availableModels.size()); i++) {
            // Show just the filename, not the full path
            std::string filename = state.availableModels[i];
            auto pos = filename.find_last_of("/\\");
            if (pos != std::string::npos) filename = filename.substr(pos + 1);

            if (ImGui::RadioButton(filename.c_str(), state.selectedModelIndex == i)) {
                state.selectedModelIndex = i;
            }
        }
    } else {
        ImGui::TextDisabled("No .obj files found");
    }

    ImGui::Separator();

    // Custom path input
    static char customPath[256] = "";
    ImGui::InputText("Path", customPath, sizeof(customPath));

    // Settings
    const char* texNames[] = { "Checkerboard", "Gradient", "White" };
    ImGui::Combo("Texture", &state.modelTexture, texNames, 3);
    ImGui::DragFloat("Scale", &state.modelScale, 0.05f, 0.01f, 50.0f);

    ImGui::Separator();

    // Load buttons
    if (!state.availableModels.empty()) {
        if (ImGui::Button("Load Selected", ImVec2(-1, 0))) {
            state.pendingModelPath = state.availableModels[state.selectedModelIndex];
            state.modelLoadRequested = true;
        }
    }

    if (customPath[0] != '\0') {
        if (ImGui::Button("Load from Path", ImVec2(-1, 0))) {
            state.pendingModelPath = customPath;
            state.modelLoadRequested = true;
        }
    }

    ImGui::End();
}

void ImGuiOverlay::buildGizmo(ImGuiState& state, std::vector<SceneObject>& objects) {
    i32 sel = state.getFirstSelected();
    if (sel < 0 || sel >= static_cast<i32>(objects.size())) {
        state.gizmoIsUsing = false;
        return;
    }

    // ─── Keyboard shortcuts (only when viewport is hovered and not renaming) ───
    if (state.viewportHovered && m_renamingIndex < 0) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) state.gizmoOperation = 0; // Translate
        if (ImGui::IsKeyPressed(ImGuiKey_E)) state.gizmoOperation = 1; // Rotate
        if (ImGui::IsKeyPressed(ImGuiKey_R)) state.gizmoOperation = 2; // Scale
    }

    // Map our int to ImGuizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (state.gizmoOperation == 1) op = ImGuizmo::ROTATE;
    if (state.gizmoOperation == 2) op = ImGuizmo::SCALE;

    SceneObject& obj = objects[sel];

    // ─── Build model matrix using ImGuizmo's own convention ───
    // This ensures compose→decompose round-trip is lossless (no Euler mismatch)
    float translation[3] = { obj.position.x, obj.position.y, obj.position.z };
    float rotation[3]    = { obj.rotation.x, obj.rotation.y, obj.rotation.z };
    float scale[3]       = { obj.scale.x,    obj.scale.y,    obj.scale.z };

    glm::mat4 modelMatrix(1.0f);
    ImGuizmo::RecomposeMatrixFromComponents(
        translation, rotation, scale,
        glm::value_ptr(modelMatrix)
    );

    // ─── Un-flip Vulkan Y-axis for ImGuizmo (it expects OpenGL conventions) ───
    glm::mat4 gizmoProj = state.projectionMatrix;
    gizmoProj[1][1] *= -1.0f;  // Undo the Vulkan Y-flip

    // Snap values
    glm::vec3 snapValues(0.0f);
    if (state.gizmoUsingSnap) {
        if (state.gizmoOperation == 0) snapValues = glm::vec3(state.gizmoSnapTranslate);
        if (state.gizmoOperation == 1) snapValues = glm::vec3(state.gizmoSnapRotate);
        if (state.gizmoOperation == 2) snapValues = glm::vec3(state.gizmoSnapScale);
    }

    // Manipulate the gizmo
    bool changed = ImGuizmo::Manipulate(
        glm::value_ptr(state.viewMatrix),
        glm::value_ptr(gizmoProj),
        op,
        ImGuizmo::LOCAL,
        glm::value_ptr(modelMatrix),
        nullptr,  // deltaMatrix
        state.gizmoUsingSnap ? glm::value_ptr(snapValues) : nullptr
    );

    state.gizmoIsUsing = ImGuizmo::IsUsing();

    // If the gizmo was changed, decompose back into position/rotation/scale
    if (changed) {
        float newTranslation[3], newRotation[3], newScale[3];
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(modelMatrix),
            newTranslation, newRotation, newScale
        );

        obj.position = glm::vec3(newTranslation[0], newTranslation[1], newTranslation[2]);
        obj.rotation = glm::vec3(newRotation[0], newRotation[1], newRotation[2]);
        obj.scale    = glm::vec3(newScale[0], newScale[1], newScale[2]);
    }
}

} // namespace Genesis
