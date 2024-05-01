/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_MESH_COMPONENT_HPP
#define HYPERION_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/utilities/UserData.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/animation/Skeleton.hpp>
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

using MeshComponentUserData = UserData<sizeof(Vec4u)>;

struct MeshComponent
{
    Handle<Mesh>            mesh;
    Handle<Material>        material;
    Handle<Skeleton>        skeleton;

    RC<RenderProxy>         proxy;
    MeshComponentFlags      flags = MESH_COMPONENT_FLAG_DIRTY;

    Matrix4                 previous_model_matrix;
};

constexpr auto x = sizeof(MeshComponent);

static_assert(sizeof(MeshComponent) == 96, "MeshComponent size must match C# struct size");

} // namespace hyperion

#endif