/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>

#include <rendering/Vertex.hpp>

namespace Hyperion {

class Mesh;
class VoxelOctree;

class MeshBuilder
{
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
