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

layout(binding = 3) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 outColor;

// Percentage-Closer Filtering (PCF) — samples a 3x3 area for soft shadow edges
float calculateShadow(vec4 posLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide (clip space → NDC)
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // Vulkan depth is already [0, 1], but x/y need mapping from [-1,1] to [0,1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // If outside the shadow map, no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;

    // Bias based on surface angle to light — steeper angles get more bias
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    // PCF: sample 3x3 grid around the shadow map texel
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0; // Average of 9 samples

    return shadow;
}

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

    // Shadow factor: 0.0 = fully lit, 1.0 = fully in shadow
    float shadow = calculateShadow(fragPosLightSpace, normal, light.lightDir);

    // Shadow affects diffuse and specular, but not ambient
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * texColor;
    outColor = vec4(result, 1.0);
}
