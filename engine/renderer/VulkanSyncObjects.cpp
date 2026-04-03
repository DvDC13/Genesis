#include "renderer/VulkanSyncObjects.h"
#include "core/Logger.h"

#include <stdexcept>

namespace Genesis {

void VulkanSyncObjects::init(VkDevice device, u32 framesInFlight, u32 imageCount) {
    m_imageAvailable.resize(framesInFlight);
    m_inFlightFences.resize(framesInFlight);
    m_renderFinished.resize(imageCount);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't block

    for (u32 i = 0; i < framesInFlight; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    for (u32 i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render finished semaphore");
        }
    }

    Logger::info("Sync objects created ({} frames in flight, {} images)", framesInFlight, imageCount);
}

void VulkanSyncObjects::shutdown(VkDevice device) {
    for (auto sem : m_imageAvailable) {
        vkDestroySemaphore(device, sem, nullptr);
    }
    for (auto fence : m_inFlightFences) {
        vkDestroyFence(device, fence, nullptr);
    }
    for (auto sem : m_renderFinished) {
        vkDestroySemaphore(device, sem, nullptr);
    }
    Logger::info("Sync objects destroyed");
}

} // namespace Genesis
