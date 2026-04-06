#version 450

// ═══════════════════════════════════════════════════════════════
// PBR Fragment Shader — Cook-Torrance GGX Microfacet BRDF
// ═══════════════════════════════════════════════════════════════

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

// Per-object PBR material (from push constants)
layout(push_constant) uniform PushConstants {
    mat4  model;      // vertex stage only
    vec3  albedo;     // base color tint
    float metallic;   // 0 = dielectric, 1 = metal
    float roughness;  // 0 = mirror, 1 = rough
    float ao;         // ambient occlusion
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
// Models the concentration of microfacets aligned with the halfway vector.
// Higher roughness → wider, dimmer highlight. Lower → sharper, brighter.
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;   // Disney's remap: alpha = roughness^2
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}

// ─── Geometry Function (Schlick-GGX, Smith's method) ───
// Models microfacet self-shadowing: how much light is blocked by
// other microfacets before reaching this one (masking + shadowing).
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;   // k for direct lighting (different from IBL)

    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1  = geometrySchlickGGX(NdotV, roughness);  // view masking
    float ggx2  = geometrySchlickGGX(NdotL, roughness);  // light shadowing
    return ggx1 * ggx2;
}

// ─── Fresnel (Schlick approximation) ───
// At grazing angles, all surfaces reflect more light (like water at sunset).
// F0 = reflectance at normal incidence (0.04 for dielectrics, albedo for metals).
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ─── Shadow (PCF — same as before) ───
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

void main() {
    // ─── Material setup ───
    vec3 texColor = texture(texSampler, fragTexCoord).rgb;
    vec3 albedo   = texColor * material.albedo;
    float metallic  = material.metallic;
    float roughness = max(material.roughness, 0.04); // avoid division by zero artifacts
    float ao        = material.ao;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(light.viewPos - fragWorldPos);
    vec3 L = normalize(light.lightDir);
    vec3 H = normalize(V + L);

    // ─── F0: surface reflectance at normal incidence ───
    // Dielectrics reflect ~4% of light (0.04), metals reflect their albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // ─── Cook-Torrance BRDF ───
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Specular: D * G * F / (4 * NdotV * NdotL)
    vec3 numerator    = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    // ─── Energy conservation ───
    // kS = Fresnel = fraction of light reflected (specular)
    // kD = 1 - kS = fraction of light refracted (diffuse)
    // Metals have no diffuse — all energy goes to specular
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    // ─── Outgoing radiance ───
    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = light.lightColor * 3.0; // light intensity (directional)
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // ─── Shadow ───
    float shadow = calculateShadow(fragPosLightSpace, N, L);

    // ─── Ambient (simple constant, could be IBL later) ───
    vec3 ambient = light.ambientStrength * albedo * ao;

    // ─── Final color ───
    vec3 color = ambient + (1.0 - shadow) * Lo;

    // HDR tone mapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));       // Reinhard tone map
    color = pow(color, vec3(1.0 / 2.2));       // Gamma correction

    outColor = vec4(color, 1.0);
}
