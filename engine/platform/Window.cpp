#include "platform/Window.h"
#include "core/Logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Genesis {

Window::Window(u32 width, u32 height, const std::string& title)
    : m_width(width), m_height(height) {

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title.c_str(),
        nullptr,
        nullptr
    );

    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);

    Logger::info("Window created: {}x{} - {}", width, height, title);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
    Logger::info("Window destroyed");
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

std::vector<const char*> Window::getRequiredVulkanExtensions() const {
    u32 count = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
    return { glfwExtensions, glfwExtensions + count };
}

void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(m_window, title.c_str());
}

bool Window::isKeyPressed(int key) const {
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

void Window::setCursorDisabled(bool disabled) {
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Window::getMouseDelta(f32& dx, f32& dy) {
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_framebufferResized = true;
    self->m_width = static_cast<u32>(width);
    self->m_height = static_cast<u32>(height);
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (self->m_firstMouse) {
        self->m_lastMouseX = xpos;
        self->m_lastMouseY = ypos;
        self->m_firstMouse = false;
        return;
    }

    self->m_mouseDeltaX += static_cast<f32>(xpos - self->m_lastMouseX);
    self->m_mouseDeltaY += static_cast<f32>(self->m_lastMouseY - ypos); // Inverted: up = positive

    self->m_lastMouseX = xpos;
    self->m_lastMouseY = ypos;
}

} // namespace Genesis
