#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos  = worldPos.xyz;

    // Transform normal by the inverse-transpose of the model matrix
    // For uniform scaling, (mat3(model)) is sufficient
    fragNormal   = mat3(transpose(inverse(ubo.model))) * inNormal;

    fragTexCoord = inTexCoord;

    gl_Position = ubo.projection * ubo.view * worldPos;
}
