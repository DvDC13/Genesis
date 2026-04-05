#version 450

layout(binding = 0) uniform ViewProjUBO {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord;

void main() {
    // Use position as the cubemap sample direction
    fragTexCoord = inPosition;

    // Remove translation from view matrix — skybox stays centered on camera
    mat4 viewNoTranslation = mat4(mat3(ubo.view));

    vec4 pos = ubo.projection * viewNoTranslation * vec4(inPosition, 1.0);

    // Set z = w so after perspective divide, depth = 1.0 (farthest possible)
    gl_Position = pos.xyww;
}
