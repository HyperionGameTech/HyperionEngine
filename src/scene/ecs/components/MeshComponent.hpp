/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_MESH_COMPONENT_HPP
#define HYPERION_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/utilities/UserData.hpp>

#include <rendering/Shader.hpp>
#include <rendering/RenderProxy.hpp>

namespace hyperion {

class Mesh;
class Material;
class Skeleton;
class BVHNode;

using MeshComponentFlags = uint32;

enum MeshComponentFlagBits : MeshComponentFlags
{
    MESH_COMPONENT_FLAG_NONE    = 0x0
};

using MeshComponentUserData = UserData<32, 16>;

HYP_STRUCT(Component, Size=240, Label="Mesh Component", Description="Controls the rendering of an entity, including the mesh, material, and skeleton.", Editor=true)
struct MeshComponent
{
    HYP_FIELD(Property="Mesh", Serialize=true, Editor=true)
    Handle<Mesh>            mesh;

    // 8

    HYP_FIELD(Property="Material", Serialize=true, Editor=true)
    Handle<Material>        material;

    // 16

    HYP_FIELD(Property="Skeleton", Serialize=true, Editor=true)
    Handle<Skeleton>        skeleton;

    // 24

    HYP_FIELD(Property="InstanceData", Serialize=true, Editor=true)
    MeshInstanceData        instance_data;

    // 128

    HYP_FIELD()
    RenderProxy             *proxy = nullptr;

    // 136

    HYP_FIELD()
    MeshComponentFlags      flags = MESH_COMPONENT_FLAG_NONE;

    // 140 + 4 padding

    HYP_FIELD()
    Matrix4                 previous_model_matrix;

    // 208

    HYP_FIELD()
    MeshComponentUserData   user_data;

    // 240

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

constexpr auto x = sizeof(RenderProxy);

} // namespace hyperion

#endif