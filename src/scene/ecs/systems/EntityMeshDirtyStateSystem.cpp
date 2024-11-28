/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RenderCommand.hpp>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityMeshDirtyStateSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity, mesh_component, transform_component] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos())) {
        // Update the material
        if (mesh_component.material.IsValid() && mesh_component.material->GetMutationState().IsDirty()) {
            // mesh_component.material->EnqueueRenderUpdates();
        }

        // If transform has changed, mark the MeshComponent as dirty
        if (mesh_component.previous_model_matrix != transform_component.transform.GetMatrix()) {
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion
