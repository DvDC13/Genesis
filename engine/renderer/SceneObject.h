#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace Genesis {

// Push constant data — sent per draw call, no buffer needed
// 64 bytes vertex (model) + 32 bytes fragment (PBR material) = 96 bytes total (within 128 byte limit)
struct PushConstantData {
    glm::mat4 model;      // 64 bytes — vertex stage
    glm::vec3 albedo;     // 12 bytes — fragment stage: base color tint
    float     metallic;   //  4 bytes — 0 = dielectric, 1 = metal
    float     roughness;  //  4 bytes — 0 = mirror, 1 = diffuse
    float     ao;         //  4 bytes — ambient occlusion
    float     _pad0;      //  4 bytes
    float     _pad1;      //  4 bytes
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

    // PBR Material properties
    glm::vec3 albedo    = glm::vec3(1.0f);  // Base color (multiplied with texture)
    f32       metallic  = 0.0f;             // 0 = dielectric (plastic), 1 = metal
    f32       roughness = 0.5f;             // 0 = mirror smooth, 1 = fully rough
    f32       ao        = 1.0f;             // Ambient occlusion (1 = no occlusion)

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
