/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/utilities/UserData.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/math/Mat4f.hpp>

#include <rendering/MeshInstanceData.hpp>

namespace hyperion {

class Mesh;
class Material;
class Skeleton;
class BVHNode;
class RenderProxyMesh;
struct MeshRaytracingData;
class LightmapVolume;

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Size = 288, Label = "Mesh Component", Description = "Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor = true)
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

    // 128

    HYP_FIELD(NoScriptBindings, Transient)
    RenderProxyMesh* UNUSED_proxy = nullptr;

    // 136

    HYP_FIELD(Transient)
    uint32 UNUSED_flags = 0;

    // 140 + 4 padding

    HYP_FIELD(Transient)
    Mat4f previousModelMatrix;

    // 208

    HYP_FIELD(NoScriptBindings, Transient)
    MeshRaytracingData* UNUSED_raytracingData = nullptr;

    // 224

    HYP_FIELD(NoScriptBindings, Transient)
    MeshComponentUserData userData;

    // 256

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    // 264

    HYP_FIELD(Transient)
    Uuid lightmapVolumeUuid = Uuid::Invalid();

    // 280

    HYP_FIELD(Transient)
    uint32 lightmapElementId = ~0u;

    MeshComponent(const Handle<Mesh>& mesh = nullptr, const Handle<Material>& material = nullptr, const Handle<Skeleton>& skeleton = nullptr)
        : mesh(mesh),
          material(material),
          skeleton(skeleton),
          instanceData(),
          previousModelMatrix(Mat4f::Identity()),
          lightmapVolumeUuid(Uuid::Invalid()),
          lightmapElementId(~0u)
    {
    }

    MeshComponent(const MeshComponent& other)
        : mesh(other.mesh),
          material(other.material),
          skeleton(other.skeleton),
          instanceData(other.instanceData),
          previousModelMatrix(other.previousModelMatrix),
          lightmapVolumeUuid(other.lightmapVolumeUuid),
          lightmapElementId(other.lightmapElementId)
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
            && instanceData == other.instanceData
            && lightmapVolumeUuid == other.lightmapVolumeUuid
            && lightmapElementId == other.lightmapElementId;
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
        hashCode.Add(lightmapVolumeUuid);
        hashCode.Add(lightmapElementId);

        return hashCode;
    }
};

} // namespace hyperion
