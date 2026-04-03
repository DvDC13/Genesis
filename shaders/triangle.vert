#version 450

// Hardcoded triangle vertices — no vertex buffer needed for Phase 1
// This lets us focus purely on the Vulkan pipeline setup

vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // top
    vec2( 0.5,  0.5),   // bottom right
    vec2(-0.5,  0.5)    // bottom left
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // red
    vec3(0.0, 1.0, 0.0),  // green
    vec3(0.0, 0.0, 1.0)   // blue
);

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
