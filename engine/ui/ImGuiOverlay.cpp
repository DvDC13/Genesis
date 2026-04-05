#include "ui/ImGuiOverlay.h"
#include "core/Logger.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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

    // Style — dark with some transparency
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 6.0f;
    style.FrameRounding    = 4.0f;
    style.GrabRounding     = 3.0f;
    style.Alpha            = 0.95f;
    style.WindowBorderSize = 1.0f;

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

    Logger::info("ImGui overlay initialized");
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
    // ─── Stats Panel ───
    if (state.showStats) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Stats", &state.showStats);

        ImGui::Text("FPS: %.0f", state.fps);
        ImGui::Text("GPU: %s", state.gpuName);
        ImGui::Separator();
        ImGui::Text("Objects:    %u", state.objectCount);
        ImGui::Text("Draw calls: %u", state.drawCalls);
        ImGui::Text("Vertices:   %u", state.vertexCount);

        ImGui::End();
    }

    // ─── Lighting Panel ───
    if (state.showLighting) {
        ImGui::SetNextWindowPos(ImVec2(10, 180), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Lighting", &state.showLighting);

        ImGui::SliderFloat3("Direction", &state.lightDir.x, -1.0f, 1.0f);
        ImGui::ColorEdit3("Color", &state.lightColor.x);
        ImGui::SliderFloat("Ambient", &state.ambientStrength, 0.0f, 1.0f);

        // Normalize light direction after editing
        float len = glm::length(state.lightDir);
        if (len > 0.001f) {
            state.lightDir /= len;
        }

        ImGui::End();
    }

    // ─── Object Inspector ───
    if (state.showObjects) {
        ImGui::SetNextWindowPos(ImVec2(10, 380), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Objects", &state.showObjects);

        // Object list
        const char* meshNames[] = { "Cube", "Sphere", "Torus" };

        for (i32 i = 0; i < static_cast<i32>(objects.size()); i++) {
            const char* meshName = (objects[i].meshIndex < 3) ? meshNames[objects[i].meshIndex] : "Unknown";
            std::string label = std::format("{}: {} ##obj{}", i, meshName, i);

            if (ImGui::Selectable(label.c_str(), state.selectedObject == i)) {
                state.selectedObject = (state.selectedObject == i) ? -1 : i;
            }
        }

        ImGui::Separator();

        // Edit selected object
        if (state.selectedObject >= 0 && state.selectedObject < static_cast<i32>(objects.size())) {
            SceneObject& obj = objects[state.selectedObject];
            ImGui::Text("Object %d", state.selectedObject);
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &obj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f, 0.01f, 20.0f);
            ImGui::DragFloat("Spin Speed", &obj.rotationSpeed, 1.0f);
        } else {
            ImGui::TextDisabled("Select an object above");
        }

        ImGui::End();
    }
}

void ImGuiOverlay::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace Genesis
