#ifndef HYPERION_V2_ECS_MESH_COMPONENT_HPP
#define HYPERION_V2_ECS_MESH_COMPONENT_HPP

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Shader.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

struct MeshComponent
{
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Handle<Shader>      shader;
};

} // namespace hyperion::v2

#endif