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

    // Generate a UV sphere (stacks = horizontal rings, sectors = vertical slices)
    static MeshData createSphere(f32 radius = 0.5f, u32 sectors = 36, u32 stacks = 18);

    // Generate a torus (ring shape)
    static MeshData createTorus(f32 majorRadius = 0.5f, f32 minorRadius = 0.2f,
                                u32 majorSegments = 36, u32 minorSegments = 24);
};

} // namespace Genesis
