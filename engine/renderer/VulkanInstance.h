#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct GLFWwindow;

namespace Genesis {

class VulkanInstance {
public:
    void init(const std::vector<const char*>& requiredExtensions, GLFWwindow* window);
    void shutdown();

    VkInstance       getInstance() const { return m_instance; }
    VkSurfaceKHR    getSurface()  const { return m_surface; }

private:
    bool checkValidationLayerSupport() const;
    void setupDebugMessenger();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData
    );

    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;
    bool                     m_enableValidation = false;

    static constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
};

} // namespace Genesis
