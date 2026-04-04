#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform LightUBO {
    vec3 lightDir;    // Direction TO the light (normalized)
    float _pad0;
    vec3 lightColor;
    float _pad1;
    vec3 viewPos;     // Camera position in world space
    float ambientStrength;
} light;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 texColor = texture(texSampler, fragTexCoord).rgb;
    vec3 normal   = normalize(fragNormal);

    // Ambient
    vec3 ambient = light.ambientStrength * light.lightColor;

    // Diffuse
    float diff   = max(dot(normal, light.lightDir), 0.0);
    vec3 diffuse = diff * light.lightColor;

    // Specular (Blinn-Phong)
    vec3 viewDir    = normalize(light.viewPos - fragWorldPos);
    vec3 halfwayDir = normalize(light.lightDir + viewDir);
    float spec      = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular   = spec * light.lightColor;

    vec3 result = (ambient + diffuse + specular) * texColor;
    outColor = vec4(result, 1.0);
}
