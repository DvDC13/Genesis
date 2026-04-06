#version 450

// Per-frame data (same for all objects)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    mat4 lightSpaceMatrix;  // Light's view-projection for shadow mapping
} ubo;

// Per-object data (changes every draw call, sent via push constants)
layout(push_constant) uniform PushConstants {
    mat4 model;          // vertex stage
    vec3 diffuseColor;   // fragment stage (passed through)
    float shininess;
    vec3 specularColor;
    float _pad0;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragPosLightSpace;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos  = worldPos.xyz;

    // Transform normal by the inverse-transpose of the model matrix
    fragNormal   = mat3(transpose(inverse(push.model))) * inNormal;

    fragTexCoord = inTexCoord;

    // Transform to light space for shadow mapping
    fragPosLightSpace = ubo.lightSpaceMatrix * worldPos;

    gl_Position = ubo.projection * ubo.view * worldPos;
}
