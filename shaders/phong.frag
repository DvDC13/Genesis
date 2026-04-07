#version 450

// ═══════════════════════════════════════════════════════════════
// PBR Fragment Shader — Cook-Torrance GGX + Multiple Point Lights
// ═══════════════════════════════════════════════════════════════

layout(binding = 1) uniform sampler2D texSampler;

struct PointLight {
    vec3  position;
    float radius;
    vec3  color;
    float intensity;
};

layout(binding = 2) uniform LightUBO {
    // Directional light
    vec3  lightDir;
    float _pad0;
    vec3  lightColor;
    float _pad1;
    vec3  viewPos;
    float ambientStrength;
    // Point lights
    uint  pointLightCount;
    float _pad2[3];
    PointLight pointLights[8];
} light;

layout(binding = 3) uniform sampler2D shadowMap;

// Per-object PBR material (from push constants)
layout(push_constant) uniform PushConstants {
    mat4  model;
    vec3  albedo;
    float metallic;
    float roughness;
    float ao;
    float _pad0;
    float _pad1;
} material;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// ─── Normal Distribution Function (GGX / Trowbridge-Reitz) ───
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}

// ─── Geometry Function (Schlick-GGX, Smith's method) ───
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// ─── Fresnel (Schlick approximation) ───
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ─── Shadow (PCF) ───
float calculateShadow(vec4 posLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

// ─── Compute PBR radiance for a single light direction ───
vec3 computeRadiance(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // ─── Material setup ───
    vec3 texColor = texture(texSampler, fragTexCoord).rgb;
    vec3 albedo   = texColor * material.albedo;
    float metallic  = material.metallic;
    float roughness = max(material.roughness, 0.04);
    float ao        = material.ao;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(light.viewPos - fragWorldPos);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ─── Directional light ───
    vec3 L_dir = normalize(light.lightDir);
    vec3 dirRadiance = light.lightColor * 3.0;
    vec3 Lo = computeRadiance(N, V, L_dir, dirRadiance, albedo, metallic, roughness, F0);

    // Apply shadow to directional light only
    float shadow = calculateShadow(fragPosLightSpace, N, L_dir);
    Lo *= (1.0 - shadow);

    // ─── Point lights ───
    for (uint i = 0; i < light.pointLightCount && i < 8; i++) {
        vec3 lightPos = light.pointLights[i].position;
        vec3 lightCol = light.pointLights[i].color;
        float intensity = light.pointLights[i].intensity;
        float radius    = light.pointLights[i].radius;

        vec3 toLight  = lightPos - fragWorldPos;
        float dist    = length(toLight);
        vec3 L_point  = toLight / dist;

        // Smooth distance attenuation with radius falloff
        float attenuation = 1.0 / (dist * dist + 1.0);
        float windowing   = max(1.0 - pow(dist / radius, 4.0), 0.0);
        attenuation *= windowing * windowing;

        vec3 pointRadiance = lightCol * intensity * attenuation;
        Lo += computeRadiance(N, V, L_point, pointRadiance, albedo, metallic, roughness, F0);
    }

    // ─── Ambient ───
    vec3 ambient = light.ambientStrength * albedo * ao;

    // ─── Final color ───
    vec3 color = ambient + Lo;

    // HDR tone mapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
