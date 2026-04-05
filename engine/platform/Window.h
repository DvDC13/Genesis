#pragma once

#include "core/Types.h"

#include <string>
#include <vector>

struct GLFWwindow;

namespace Genesis {

class Window {
public:
    Window(u32 width, u32 height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void pollEvents() const;

    GLFWwindow* getHandle() const { return m_window; }
    u32 getWidth() const { return m_width; }
    u32 getHeight() const { return m_height; }
    bool wasResized() const { return m_framebufferResized; }
    void resetResizedFlag() { m_framebufferResized = false; }

    std::vector<const char*> getRequiredVulkanExtensions() const;
    void setTitle(const std::string& title);

    // Input
    bool isKeyPressed(int key) const;
    void setCursorDisabled(bool disabled);
    void getMouseDelta(f32& dx, f32& dy);

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    GLFWwindow* m_window = nullptr;
    u32 m_width;
    u32 m_height;
    bool m_framebufferResized = false;

    // Mouse tracking
    f64 m_lastMouseX  = 0.0;
    f64 m_lastMouseY  = 0.0;
    f32 m_mouseDeltaX = 0.0f;
    f32 m_mouseDeltaY = 0.0f;
    bool m_firstMouse = true;
};

} // namespace Genesis
