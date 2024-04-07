#ifndef HYPERION_V2_ECS_MESH_COMPONENT_HPP
#define HYPERION_V2_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/lib/UserData.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/animation/Skeleton.hpp>
#include <rendering/Shader.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

using MeshComponentFlags = uint32;

enum MeshComponentFlagBits : MeshComponentFlags
{
    MESH_COMPONENT_FLAG_NONE    = 0x0,
    MESH_COMPONENT_FLAG_INIT    = 0x1,
    MESH_COMPONENT_FLAG_DIRTY   = 0x2
};

using MeshComponentUserData = UserData<sizeof(Vec4u)>;

struct MeshComponent
{
    Handle<Mesh>            mesh;
    Handle<Material>        material;
    Handle<Skeleton>        skeleton;

    MeshComponentUserData   user_data;

    MeshComponentFlags      flags = MESH_COMPONENT_FLAG_DIRTY;

    Matrix4                 previous_model_matrix;
};

static_assert(sizeof(MeshComponent) == 96, "MeshComponent size must match C# struct size");

} // namespace hyperion::v2

#endif