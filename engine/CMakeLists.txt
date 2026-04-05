add_library(GenesisEngine STATIC
    core/Logger.cpp
    platform/Window.cpp
    renderer/VulkanInstance.cpp
    renderer/VulkanDevice.cpp
    renderer/VulkanSwapchain.cpp
    renderer/VulkanPipeline.cpp
    renderer/VulkanCommandPool.cpp
    renderer/VulkanSyncObjects.cpp
    renderer/VulkanBuffer.cpp
    renderer/VulkanDescriptors.cpp
    renderer/VulkanTexture.cpp
    renderer/Mesh.cpp
    renderer/ModelLoader.cpp
    renderer/Camera.cpp
    renderer/Renderer.cpp
    ui/ImGuiOverlay.cpp
)

target_include_directories(GenesisEngine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find stb and tinyobjloader from vcpkg
find_package(Stb REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_include_directories(GenesisEngine
    PRIVATE ${Stb_INCLUDE_DIR}
)

target_link_libraries(GenesisEngine
    PUBLIC  Vulkan::Vulkan
    PUBLIC  glfw
    PUBLIC  glm::glm
    PRIVATE tinyobjloader::tinyobjloader
    PUBLIC  imgui::imgui
)

# Enable validation layers in debug builds
# GLM: use radians and Vulkan's [0,1] depth range instead of OpenGL's [-1,1]
target_compile_definitions(GenesisEngine
    PUBLIC $<$<CONFIG:Debug>:GENESIS_DEBUG>
    PUBLIC GLM_FORCE_RADIANS
    PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE
)
