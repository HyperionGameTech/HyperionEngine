/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Vertex.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Mesh;
class VoxelOctree;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex& operator[](uint32 index)
    {
        return vertices[index];
    }

    constexpr const Vertex& operator[](uint32 index) const
    {
        return vertices[index];
    }
};

class HYP_API MeshBuilder
{
    static const Array<Vertex> quadVertices;
    static const Array<uint32> quadIndices;
    static const Array<Vertex> cubeVertices;

public:
    static Handle<Mesh> Quad();
    static Handle<Mesh> Cube();
    static Handle<Mesh> NormalizedCubeSphere(uint32 numDivisions);

    static Handle<Mesh> ApplyTransform(const Mesh* mesh, const Transform& transform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b);

    static Handle<Mesh> BuildVoxelMesh(const VoxelOctree& voxelOctree);
};

} // namespace hyperion
