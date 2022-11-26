#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/Mesh.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/Handle.hpp>
#include <math/BoundingBox.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Topology;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex &operator[](UInt index) { return vertices[index]; }
    constexpr const Vertex &operator[](UInt index) const { return vertices[index]; }
};

class MeshBuilder
{
    static const std::vector<Vertex> quad_vertices;
    static const std::vector<Mesh::Index> quad_indices;
    static const std::vector<Vertex> cube_vertices;

public:
    struct Voxel
    {
        BoundingBox aabb;
        bool filled;

        Voxel()
            : aabb(BoundingBox()), filled(false) {}
        Voxel(const BoundingBox &aabb, bool filled = false)
            : aabb(aabb), filled(filled) {}
        Voxel(const Voxel &other)
            : aabb(other.aabb), filled(other.filled) {}
    };

    struct VoxelGrid
    {
        Array<Voxel> voxels;
        UInt size_x, size_y, size_z;
        Float voxel_size;
    };

    static Handle<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(UInt num_divisions);

    static Handle<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b);

    static VoxelGrid Voxelize(const Mesh *mesh, float voxel_size);
    static Handle<Mesh> BuildVoxelMesh(VoxelGrid &&voxel_grid);
};

} // namespace hyperion::v2

#endif
