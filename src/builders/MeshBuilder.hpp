#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/Mesh.hpp>

#include <util/Defines.hpp>

namespace hyperion::v2 {

using renderer::Topology;

class MeshBuilder {
public:
    static std::unique_ptr<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static std::unique_ptr<Mesh> Cube();
};

} // namespace hyperion::v2

#endif
