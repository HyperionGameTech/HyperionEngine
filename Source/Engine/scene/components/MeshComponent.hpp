/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/utilities/UserData.hpp>
#include <Core/utilities/Uuid.hpp>

#include <Core/math/Mat4f.hpp>

#include <rendering/MeshInstanceData.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class BVHNode;
class RenderProxyMesh;
struct MeshRayTracingData;
class LightmapVolume;

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Size = 208, Label = "Mesh Component", Description = "Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor = true)
struct MeshComponent
{
    HYP_STRUCT_BODY(MeshComponent);

    HYP_FIELD(Property = "Mesh", Editor = true)
    Handle<Mesh> mesh;

    // 8

    HYP_FIELD(Property = "Material", Editor = true)
    Handle<Material> material;

    // 16

    HYP_FIELD(Property = "Skeleton", Editor = true)
    Handle<Skeleton> skeleton;

    // 24

    HYP_FIELD(Property = "InstanceData", Editor = true)
    MeshInstanceData instanceData;

    // 112

    HYP_FIELD(Transient)
    Mat4f previousModelMatrix;

    // 176

    HYP_FIELD(NoScriptBindings, Transient)
    MeshComponentUserData userData;

    // 208

    MeshComponent(const Handle<Mesh>& mesh = nullptr, const Handle<Material>& material = nullptr, const Handle<Skeleton>& skeleton = nullptr)
        : mesh(mesh),
          material(material),
          skeleton(skeleton),
          instanceData(),
          previousModelMatrix(Mat4f::Identity())
    {
    }

    MeshComponent(const MeshComponent& other)
        : mesh(other.mesh),
          material(other.material),
          skeleton(other.skeleton),
          instanceData(other.instanceData),
          previousModelMatrix(other.previousModelMatrix)
    {
    }

    HYP_API MeshComponent& operator=(const MeshComponent& other);

    HYP_API MeshComponent(MeshComponent&& other) noexcept;
    HYP_API MeshComponent& operator=(MeshComponent&& other) noexcept;

    HYP_API ~MeshComponent();

    HYP_FORCE_INLINE bool operator==(const MeshComponent& other) const
    {
        return mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && instanceData == other.instanceData;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshComponent& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return mesh.IsValid() && material.IsValid();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(mesh);
        hashCode.Add(material);
        hashCode.Add(skeleton);
        hashCode.Add(instanceData);

        return hashCode;
    }
};

} // namespace Hyperion
