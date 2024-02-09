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
        Array<Voxel>    voxels;
        uint            size_x, size_y, size_z;
        float           voxel_size;

        uint GetIndex(uint x, uint y, uint z) const
        {
            return (x * size_y * size_z) + (y * size_z) + z;
        }
    };

    static Handle<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(uint num_divisions);

    static Handle<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b);

    static VoxelGrid Voxelize(const Mesh *mesh, float voxel_size);
    static VoxelGrid Voxelize(const Mesh *mesh, Vec3u voxel_grid_size);
    static Handle<Mesh> BuildVoxelMesh(VoxelGrid voxel_grid);
};

} // namespace hyperion::v2

#endif
