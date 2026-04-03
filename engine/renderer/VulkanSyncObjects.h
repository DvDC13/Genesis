#pragma once

#include "core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Genesis {

class VulkanSyncObjects {
public:
    void init(VkDevice device, u32 framesInFlight, u32 imageCount);
    void shutdown(VkDevice device);

    // Indexed by m_currentFrame (frames in flight)
    VkSemaphore getImageAvailable(u32 frame) const { return m_imageAvailable[frame]; }
    VkFence     getInFlightFence(u32 frame)  const { return m_inFlightFences[frame]; }

    // Indexed by acquired imageIndex (per swapchain image)
    // This prevents reusing a semaphore the presentation engine still holds
    VkSemaphore getRenderFinished(u32 imageIndex) const { return m_renderFinished[imageIndex]; }

private:
    std::vector<VkSemaphore> m_imageAvailable;  // per frame-in-flight
    std::vector<VkFence>     m_inFlightFences;   // per frame-in-flight
    std::vector<VkSemaphore> m_renderFinished;   // per swapchain image
};

} // namespace Genesis
