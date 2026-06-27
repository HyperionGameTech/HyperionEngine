/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/UserData.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Asset/AssetReference.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class BVHNode;
struct RenderProxyMesh;
struct MeshRayTracingData;
class LightmapVolume;
class InstancedMeshData;

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Label = "Mesh Component", Description = "Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor = true)
struct ENGINE_API MeshComponent
{
    HYP_STRUCT_BODY(MeshComponent);

    HYP_FIELD(Property = "Mesh")
    Handle<Mesh> mesh;

    HYP_FIELD(Property = "Material")
    Handle<Material> material;

    HYP_FIELD(Property = "Skeleton")
    Handle<Skeleton> skeleton;

    HYP_FIELD(Property = "EnableAutoInstancing", Serialize, EditHide, NoScriptBindings)
    bool enableAutoInstancing = false;

    HYP_FIELD(Property = "InstanceData", Serialize, EditHide, NoScriptBindings)
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
