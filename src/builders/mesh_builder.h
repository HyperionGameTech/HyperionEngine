#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/mesh.h>

#include <util/defines.h>

namespace hyperion::v2 {

using renderer::Topology;

class MeshBuilder {
public:
    static std::unique_ptr<Mesh> Quad(Topology topology = Topology::TRIANGLES);
};

} // namespace hyperion::v2

#endif
