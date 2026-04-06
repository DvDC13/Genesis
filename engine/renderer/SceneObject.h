#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace Genesis {

// Push constant data — sent per draw call, no buffer needed
// 64 bytes vertex (model) + 32 bytes fragment (material) = 96 bytes total (within 128 byte limit)
struct PushConstantData {
    glm::mat4 model;       // 64 bytes — vertex stage
    glm::vec3 diffuseColor;  // 12 bytes — fragment stage
    float     shininess;     //  4 bytes
    glm::vec3 specularColor; // 12 bytes
    float     _pad0;         //  4 bytes (alignment)
};

class SceneObject {
public:
    std::string name;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles in degrees (pitch, yaw, roll)
    glm::vec3 scale    = glm::vec3(1.0f);

    // Which mesh and texture this object uses (index into Renderer's arrays)
    u32 meshIndex    = 0;
    u32 textureIndex = 0;

    // Material properties
    glm::vec3 diffuseColor  = glm::vec3(1.0f);  // Multiplied with texture color
    glm::vec3 specularColor = glm::vec3(1.0f);  // Specular highlight tint
    f32       shininess     = 32.0f;             // Specular exponent (higher = sharper)

    // Optional per-frame animation
    f32 rotationSpeed = 0.0f; // degrees per second around Y axis

    void update(f32 deltaTime) {
        if (rotationSpeed != 0.0f) {
            rotation.y += rotationSpeed * deltaTime;
        }
    }

    glm::mat4 getModelMatrix() const {
        // Rotation order: X * Y * Z (matches ImGuizmo convention)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

} // namespace Genesis
