#include "renderer/Camera.h"

#include <algorithm>

namespace Genesis {

Camera::Camera(glm::vec3 position)
    : m_position(position)
    , m_worldUp(0.0f, 1.0f, 0.0f)
{
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix(f32 aspect) const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), aspect, m_nearPlane, m_farPlane);
    // Vulkan has inverted Y compared to OpenGL — flip it
    proj[1][1] *= -1.0f;
    return proj;
}

void Camera::processKeyboard(CameraDirection direction, f32 deltaTime) {
    f32 velocity = m_speed * deltaTime;

    switch (direction) {
        case CameraDirection::Forward:  m_position += m_front * velocity; break;
        case CameraDirection::Backward: m_position -= m_front * velocity; break;
        case CameraDirection::Left:     m_position -= m_right * velocity; break;
        case CameraDirection::Right:    m_position += m_right * velocity; break;
    }
}

void Camera::processMouseMovement(f32 xOffset, f32 yOffset) {
    xOffset *= m_sensitivity;
    yOffset *= m_sensitivity;

    m_yaw   += xOffset;
    m_pitch += yOffset;

    // Clamp pitch to prevent flipping
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateVectors();
}

void Camera::updateVectors() {
    // Calculate new front vector from yaw and pitch (Euler angles)
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    // Recalculate right and up
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));
}

} // namespace Genesis
