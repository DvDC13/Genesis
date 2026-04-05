#version 450

// Light's view-projection matrix
layout(binding = 0) uniform LightVP {
    mat4 lightSpaceMatrix;
} lightVP;

// Per-object model matrix (push constant)
layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // unused, but must match vertex format
layout(location = 2) in vec2 inTexCoord;  // unused, but must match vertex format

void main() {
    gl_Position = lightVP.lightSpaceMatrix * push.model * vec4(inPosition, 1.0);
}
