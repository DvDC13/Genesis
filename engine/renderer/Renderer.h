#pragma once

#include "renderer/VulkanInstance.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanPipeline.h"
#include "renderer/VulkanCommandPool.h"
#include "renderer/VulkanSyncObjects.h"
#include "core/Types.h"

namespace Genesis {

class Window;

class Renderer {
public:
    void init(Window& window);
    void drawFrame();
    void shutdown();

private:
    void recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex);
    void recreateSwapchain();

    Window* m_window = nullptr;

    VulkanInstance    m_instance;
    VulkanDevice      m_device;
    VulkanSwapchain   m_swapchain;
    VulkanPipeline    m_pipeline;
    VulkanCommandPool m_commandPool;
    VulkanSyncObjects m_syncObjects;

    u32 m_currentFrame = 0;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Genesis
