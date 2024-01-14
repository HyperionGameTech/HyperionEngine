#ifndef HYPERION_V2_ECS_MESH_COMPONENT_HPP
#define HYPERION_V2_ECS_MESH_COMPONENT_HPP

#include <core/Handle.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Shader.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

using MeshComponentFlags = UInt32;

enum MeshComponentFlagBits : MeshComponentFlags
{
    MESH_COMPONENT_FLAG_NONE    = 0x0,
    MESH_COMPONENT_FLAG_INIT    = 0x1,
    MESH_COMPONENT_FLAG_DIRTY   = 0x2
};

struct MeshComponent
{
    Handle<Mesh>        mesh;
    Handle<Material>    material;

    MeshComponentFlags  flags = MESH_COMPONENT_FLAG_DIRTY;
};

} // namespace hyperion::v2

#endif