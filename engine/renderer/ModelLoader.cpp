#include "renderer/ModelLoader.h"
#include "core/Logger.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <unordered_map>
#include <stdexcept>

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

} // namespace Genesis
