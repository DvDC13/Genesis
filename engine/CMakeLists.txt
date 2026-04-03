add_library(GenesisEngine STATIC
    core/Logger.cpp
    platform/Window.cpp
    renderer/VulkanInstance.cpp
    renderer/VulkanDevice.cpp
    renderer/VulkanSwapchain.cpp
    renderer/VulkanPipeline.cpp
    renderer/VulkanCommandPool.cpp
    renderer/VulkanSyncObjects.cpp
    renderer/Renderer.cpp
)

target_include_directories(GenesisEngine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GenesisEngine
    PUBLIC  Vulkan::Vulkan
    PUBLIC  glfw
    PUBLIC  glm::glm
)

# Enable validation layers in debug builds
target_compile_definitions(GenesisEngine
    PUBLIC $<$<CONFIG:Debug>:GENESIS_DEBUG>
)
