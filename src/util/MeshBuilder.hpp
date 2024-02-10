#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/Mesh.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/Handle.hpp>
#include <math/BoundingBox.hpp>
#include <math/Vector3.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Topology;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex &operator[](uint index)
        { return vertices[index]; }

    constexpr const Vertex &operator[](uint index) const
        { return vertices[index]; }
};

class MeshBuilder
{
    static const Array<Vertex>      quad_vertices;
    static const Array<Mesh::Index> quad_indices;
    static const Array<Vertex>      cube_vertices;

public:
    static Handle<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(uint num_divisions);

    static Handle<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b);
};

} // namespace hyperion::v2

#endif
