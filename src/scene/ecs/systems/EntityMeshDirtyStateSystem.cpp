/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/BVH.hpp>

#include <core/containers/HashSet.hpp>

#include <rendering/backend/RenderCommand.hpp>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityMeshDirtyStateSystem::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updated_entities;

    for (auto [entity, mesh_component, transform_component] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        // Update the material
        if (mesh_component.material.IsValid() && mesh_component.material->GetMutationState().IsDirty())
        {
            mesh_component.material->EnqueueRenderUpdates();
        }

        // If transform has changed, mark the MeshComponent as dirty
        if (mesh_component.previous_model_matrix != transform_component.transform.GetMatrix())
        {
            updated_entities.Insert(entity->WeakHandleFromThis());
        }
    }

    if (updated_entities.Any())
    {
        AfterProcess([this, updated_entities = std::move(updated_entities)]()
            {
                for (const WeakHandle<Entity>& entity_weak : updated_entities)
                {
                    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity_weak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
