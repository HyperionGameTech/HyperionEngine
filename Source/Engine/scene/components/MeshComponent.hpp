/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/utilities/UserData.hpp>
#include <Core/utilities/Uuid.hpp>

#include <Core/math/Mat4f.hpp>

#include <asset/AssetReference.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class BVHNode;
class RenderProxyMesh;
struct MeshRayTracingData;
class LightmapVolume;
class InstancedMeshData;

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Label = "Mesh Component", Description = "Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor = true)
struct MeshComponent
{
    HYP_STRUCT_BODY(MeshComponent);

    HYP_FIELD(Property = "Mesh", Editor)
    Handle<Mesh> mesh;

    HYP_FIELD(Property = "Material", Editor)
    Handle<Material> material;

    HYP_FIELD(Property = "Skeleton", Editor)
    Handle<Skeleton> skeleton;

    HYP_FIELD(Property = "EnableAutoInstancing", Serialize)
    bool enableAutoInstancing = false;

    HYP_FIELD(Property = "InstanceData", Serialize, NoScriptBindings)
    AssetReference instanceData;

    HYP_FIELD(Transient)
    uint32 numInstances = 0;

    HYP_FIELD(Transient)
    Mat4f previousModelMatrix;

    HYP_FIELD(NoScriptBindings, Transient)
    MeshComponentUserData userData;

    MeshComponent(const Handle<Mesh>& mesh = nullptr, const Handle<Material>& material = nullptr, const Handle<Skeleton>& skeleton = nullptr)
        : mesh(mesh),
          material(material),
          skeleton(skeleton),
          instanceData(),
          previousModelMatrix(Mat4f::Identity())
    {
    }

    MeshComponent(const MeshComponent& other) = default;
    MeshComponent& operator=(const MeshComponent& other);

    MeshComponent(MeshComponent&& other) noexcept = default;
    MeshComponent& operator=(MeshComponent&& other) noexcept;

    ~MeshComponent();
};

} // namespace Hyperion
