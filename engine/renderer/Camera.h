#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Genesis {

enum class CameraDirection {
    Forward,
    Backward,
    Left,
    Right
};

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(f32 aspect) const;

    void processKeyboard(CameraDirection direction, f32 deltaTime);
    void processMouseMovement(f32 xOffset, f32 yOffset);

    glm::vec3 getPosition() const { return m_position; }

private:
    void updateVectors();

    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    f32 m_yaw         = -90.0f;  // Look along -Z initially
    f32 m_pitch       = 0.0f;
    f32 m_speed       = 3.0f;
    f32 m_sensitivity = 0.1f;
    f32 m_fov         = 45.0f;
    f32 m_nearPlane   = 0.1f;
    f32 m_farPlane    = 100.0f;
};

} // namespace Genesis
