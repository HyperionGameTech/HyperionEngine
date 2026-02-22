/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/containers/Array.hpp>

#include <core/reflection/Handle.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Vertex.hpp>

namespace Hyperion {

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
    /*! \brief Build a quad mesh in the XY plane with size 1x1. */
    static Handle<Mesh> Quad();

    /*! \brief Build a cube mesh.
     *  \param originOnBottom If true, the cube's origin will be at the bottom face center. Otherwise, it will be at the center of the cube.
     */
    static Handle<Mesh> Cube(bool originOnBottom = false);

    /*! \brief Build a UV sphere mesh with \p numSlices slices and \p numStacks stacks.
     *  \param numSlices Number of slices (longitude divisions).
     *  \param numStacks Number of stacks (latitude divisions).
     */
    static Handle<Mesh> NormalizedCubeSphere(uint32 numDivisions);

    static Handle<Mesh> ApplyTransform(const Mesh* mesh, const Transform& transform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform);
    static Handle<Mesh> Merge(const Mesh* a, const Mesh* b);

    static Handle<Mesh> BuildVoxelMesh(const VoxelOctree& voxelOctree);
};

} // namespace Hyperion
