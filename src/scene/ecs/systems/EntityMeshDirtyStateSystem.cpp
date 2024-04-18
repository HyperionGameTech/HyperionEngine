/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <Engine.hpp>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);
}

void EntityMeshDirtyStateSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, mesh_component, transform_component] : entity_manager.GetEntitySet<MeshComponent, TransformComponent>()) {
        // Update the material
        if (mesh_component.material.IsValid() && mesh_component.material->GetMutationState().IsDirty()) {
            mesh_component.material->EnqueueRenderUpdates();
        }

        // If transform has changed, mark the MeshComponent as dirty
        if (mesh_component.previous_model_matrix != transform_component.transform.GetMatrix()) {
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion
