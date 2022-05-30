#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/mesh.h>

#include <util/defines.h>

namespace hyperion::v2 {

using renderer::Topology;

class MeshBuilder {
public:
#if !HYP_APPLE
    static std::unique_ptr<Mesh> Quad(Topology topology);
#else
    static std::unique_ptr<Mesh> Quad();
#endif
};

} // namespace hyperion::v2

#endif
