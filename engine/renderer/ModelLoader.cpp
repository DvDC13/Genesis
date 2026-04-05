#include "renderer/ModelLoader.h"
#include "core/Logger.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <unordered_map>
#include <stdexcept>
#include <cmath>

namespace Genesis {

// Hash function for deduplicating vertices
struct VertexHash {
    size_t operator()(const Vertex& v) const {
        size_t h = 0;
        auto hashFloat = [&h](float f) {
            h ^= std::hash<float>{}(f) + 0x9e3779b9 + (h << 6) + (h >> 2);
        };
        hashFloat(v.position.x); hashFloat(v.position.y); hashFloat(v.position.z);
        hashFloat(v.normal.x);   hashFloat(v.normal.y);   hashFloat(v.normal.z);
        hashFloat(v.texCoord.x); hashFloat(v.texCoord.y);
        return h;
    }
};

struct VertexEqual {
    bool operator()(const Vertex& a, const Vertex& b) const {
        return a.position == b.position && a.normal == b.normal && a.texCoord == b.texCoord;
    }
};

MeshData ModelLoader::loadOBJ(const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error("Failed to load OBJ: " + warn + err);
    }

    if (!warn.empty()) {
        Logger::warn("OBJ loader: {}", warn);
    }

    MeshData data;
    std::unordered_map<Vertex, u32, VertexHash, VertexEqual> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V for Vulkan
                };
            }

            auto it = uniqueVertices.find(vertex);
            if (it == uniqueVertices.end()) {
                u32 newIndex = static_cast<u32>(data.vertices.size());
                uniqueVertices[vertex] = newIndex;
                data.vertices.push_back(vertex);
                data.indices.push_back(newIndex);
            } else {
                data.indices.push_back(it->second);
            }
        }
    }

    Logger::info("OBJ loaded: {} ({} vertices, {} indices)",
                 filepath, data.vertices.size(), data.indices.size());
    return data;
}

MeshData ModelLoader::createCube() {
    MeshData data;

    // 24 vertices: 4 per face, each with position + outward normal + UV
    // Counter-clockwise winding when viewed from outside
    data.vertices = {
        // Front face (normal: +Z)
        {{ -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }},

        // Back face (normal: -Z)
        {{  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }},
        {{  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }},

        // Top face (normal: +Y)
        {{ -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }},

        // Bottom face (normal: -Y)
        {{ -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }},

        // Right face (normal: +X)
        {{  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }},
        {{  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }},

        // Left face (normal: -X)
        {{ -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }},
    };

    // 36 indices: 2 triangles per face, 6 faces
    for (u32 face = 0; face < 6; face++) {
        u32 base = face * 4;
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 3);
    }

    Logger::info("Cube mesh created (24 vertices, 36 indices)");
    return data;
}

MeshData ModelLoader::createSphere(f32 radius, u32 sectors, u32 stacks) {
    MeshData data;

    const f32 PI = glm::pi<f32>();

    // Generate vertices: loop from top pole to bottom pole
    for (u32 i = 0; i <= stacks; i++) {
        f32 stackAngle = PI / 2.0f - PI * static_cast<f32>(i) / static_cast<f32>(stacks); // from +90 to -90
        f32 xy = radius * std::cos(stackAngle);
        f32 z  = radius * std::sin(stackAngle);

        for (u32 j = 0; j <= sectors; j++) {
            f32 sectorAngle = 2.0f * PI * static_cast<f32>(j) / static_cast<f32>(sectors);

            Vertex v{};
            v.position = {
                xy * std::cos(sectorAngle),
                z,
                xy * std::sin(sectorAngle)
            };
            // Normal = normalized position (for a sphere centered at origin)
            v.normal = glm::normalize(v.position);
            // UV: horizontal = sector, vertical = stack
            v.texCoord = {
                static_cast<f32>(j) / static_cast<f32>(sectors),
                static_cast<f32>(i) / static_cast<f32>(stacks)
            };
            data.vertices.push_back(v);
        }
    }

    // Generate indices: two triangles per quad
    for (u32 i = 0; i < stacks; i++) {
        u32 k1 = i * (sectors + 1);
        u32 k2 = k1 + sectors + 1;

        for (u32 j = 0; j < sectors; j++, k1++, k2++) {
            // Top triangle (skip for the top pole)
            if (i != 0) {
                data.indices.push_back(k1);
                data.indices.push_back(k2);
                data.indices.push_back(k1 + 1);
            }
            // Bottom triangle (skip for the bottom pole)
            if (i != (stacks - 1)) {
                data.indices.push_back(k1 + 1);
                data.indices.push_back(k2);
                data.indices.push_back(k2 + 1);
            }
        }
    }

    Logger::info("Sphere mesh created ({} vertices, {} indices)",
                 data.vertices.size(), data.indices.size());
    return data;
}

MeshData ModelLoader::createTorus(f32 majorRadius, f32 minorRadius,
                                   u32 majorSegments, u32 minorSegments) {
    MeshData data;

    const f32 PI = glm::pi<f32>();

    // Generate vertices
    for (u32 i = 0; i <= majorSegments; i++) {
        f32 u = 2.0f * PI * static_cast<f32>(i) / static_cast<f32>(majorSegments);
        f32 cosU = std::cos(u);
        f32 sinU = std::sin(u);

        for (u32 j = 0; j <= minorSegments; j++) {
            f32 v = 2.0f * PI * static_cast<f32>(j) / static_cast<f32>(minorSegments);
            f32 cosV = std::cos(v);
            f32 sinV = std::sin(v);

            Vertex vert{};
            vert.position = {
                (majorRadius + minorRadius * cosV) * cosU,
                minorRadius * sinV,
                (majorRadius + minorRadius * cosV) * sinU
            };

            // Normal: direction from the center of the tube ring to this point
            glm::vec3 center = { majorRadius * cosU, 0.0f, majorRadius * sinU };
            vert.normal = glm::normalize(vert.position - center);

            vert.texCoord = {
                static_cast<f32>(i) / static_cast<f32>(majorSegments),
                static_cast<f32>(j) / static_cast<f32>(minorSegments)
            };

            data.vertices.push_back(vert);
        }
    }

    // Generate indices
    for (u32 i = 0; i < majorSegments; i++) {
        for (u32 j = 0; j < minorSegments; j++) {
            u32 k1 = i * (minorSegments + 1) + j;
            u32 k2 = k1 + minorSegments + 1;

            data.indices.push_back(k1);
            data.indices.push_back(k2);
            data.indices.push_back(k1 + 1);

            data.indices.push_back(k1 + 1);
            data.indices.push_back(k2);
            data.indices.push_back(k2 + 1);
        }
    }

    Logger::info("Torus mesh created ({} vertices, {} indices)",
                 data.vertices.size(), data.indices.size());
    return data;
}

} // namespace Genesis
