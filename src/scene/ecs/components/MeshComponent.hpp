/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_MESH_COMPONENT_HPP
#define HYPERION_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/utilities/UserData.hpp>

#include <scene/animation/Skeleton.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderProxy.hpp>

namespace hyperion {

struct RenderProxy;

using MeshComponentFlags = uint32;

enum MeshComponentFlagBits : MeshComponentFlags
{
    MESH_COMPONENT_FLAG_NONE    = 0x0,
    MESH_COMPONENT_FLAG_DIRTY   = 0x1
};

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Size=240, Label="Mesh Component", Description="Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor=true)
struct MeshComponent
{
    HYP_FIELD(Property="Mesh", Serialize=true, Editor=true)
    Handle<Mesh>            mesh;

    HYP_FIELD(Property="Material", Serialize=true, Editor=true)
    Handle<Material>        material;

    HYP_FIELD(Property="Skeleton", Serialize=true, Editor=true)
    Handle<Skeleton>        skeleton;

    HYP_FIELD(Property="InstanceData", Serialize=true, Editor=true)
    MeshInstanceData        instance_data;

    HYP_FIELD()
    RC<RenderProxy>         proxy;

    HYP_FIELD()
    MeshComponentFlags      flags = MESH_COMPONENT_FLAG_DIRTY;

    HYP_FIELD()
    Matrix4                 previous_model_matrix;

    HYP_FIELD()
    MeshComponentUserData   user_data;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(mesh);
        hash_code.Add(material);
        hash_code.Add(skeleton);
        hash_code.Add(instance_data);

        return hash_code;
    }
};

} // namespace hyperion

#endif