#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Genesis {

// Push constant data — sent per draw call, no buffer needed
struct PushConstantData {
    glm::mat4 model;
};

class SceneObject {
public:
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles in degrees (pitch, yaw, roll)
    glm::vec3 scale    = glm::vec3(1.0f);

    // Which mesh and texture this object uses (index into Renderer's arrays)
    u32 meshIndex    = 0;
    u32 textureIndex = 0;

    // Optional per-frame animation
    f32 rotationSpeed = 0.0f; // degrees per second around Y axis

    void update(f32 deltaTime) {
        if (rotationSpeed != 0.0f) {
            rotation.y += rotationSpeed * deltaTime;
        }
    }

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

} // namespace Genesis
