#pragma once

#include "renderer/Vertex.h"
#include "core/Types.h"

#include <vector>
#include <string>

namespace Genesis {

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32>    indices;
};

class ModelLoader {
public:
    static MeshData loadOBJ(const std::string& filepath);

    // Generate a textured cube with normals and UVs
    static MeshData createCube();
};

} // namespace Genesis
