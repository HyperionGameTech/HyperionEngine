/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_BUILDER_HPP
#define HYPERION_MESH_BUILDER_HPP

#include <core/containers/Array.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Vertex.hpp>

#include <Types.hpp>

namespace hyperion {

class Mesh;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex &operator[](uint32 index)
        { return vertices[index]; }

    constexpr const Vertex &operator[](uint32 index) const
        { return vertices[index]; }
};

struct VoxelData
{
    Vec4f   color;
};

struct Voxel
{
    BoundingBox aabb;
    bool        filled = false;

    VoxelData   data;
};

struct VoxelGrid
{
    Array<Voxel>    voxels;
    Vec3u           size;
    float           voxel_size;

    VoxelGrid()
        : size(Vec3u::Zero()),
          voxel_size(0.25f)
    {
    }

    VoxelGrid(Vec3u size, float voxel_size = 0.25f)
        : size(size),
          voxel_size(voxel_size)
    {
        voxels.Resize(size.x * size.y * size.z);

        for (uint32 x = 0; x < size.x; x++) {
            for (uint32 y = 0; y < size.y; y++) {
                for (uint32 z = 0; z < size.z; z++) {
                    voxels[GetIndex(x, y, z)] = Voxel {
                        BoundingBox {
                            Vec3f(float(x), float(y), float(z)) * voxel_size,
                            Vec3f(float(x) + 1, float(y) + 1, float(z) + 1) * voxel_size
                        }
                    };
                }
            }
        }
    }

    VoxelGrid(const VoxelGrid &other)                   = default;
    VoxelGrid &operator=(const VoxelGrid &other)        = default;
    VoxelGrid(VoxelGrid &&other) noexcept               = default;
    VoxelGrid &operator=(VoxelGrid &&other) noexcept    = default;
    ~VoxelGrid()                                        = default;

    uint32 GetIndex(uint32 x, uint32 y, uint32 z) const
        { return (x * size.y * size.z) + (y * size.z) + z; }

    const Voxel &GetVoxel(uint32 x, uint32 y, uint32 z) const
    {
        const uint32 index = GetIndex(x, y, z);
        AssertThrowMsg(index < voxels.Size(), "Voxel index out of bounds");

        return voxels[index];
    }

    void SetVoxel(uint32 x, uint32 y, uint32 z, VoxelData data)
    {
        const uint32 index = GetIndex(x, y, z);
        AssertThrowMsg(index < voxels.Size(), "Voxel index out of bounds");

        voxels[index].filled = true;
        voxels[index].data = data;
    }
};

class HYP_API MeshBuilder
{
    static const Array<Vertex>  quad_vertices;
    static const Array<uint32>  quad_indices;
    static const Array<Vertex>  cube_vertices;

public:
    static Handle<Mesh> Quad();
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(uint32 num_divisions);

    static Handle<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static Handle<Mesh> Merge(const Mesh *a, const Mesh *b);

    static Handle<Mesh> BuildVoxelMesh(VoxelGrid voxel_grid);
};

} // namespace hyperion

#endif
