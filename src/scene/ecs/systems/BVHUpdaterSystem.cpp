/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BVHUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Mesh.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(ECS);

void BVHUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    BVHComponent& bvh_component = GetEntityManager().GetComponent<BVHComponent>(entity);
    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent& transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid())
    {
        bvh_component.bvh = BVHNode();

        return;
    }

    if (mesh_component.mesh->BuildBVH(bvh_component.bvh, /* max_depth */ 3))
    {
        HYP_LOG(ECS, Info, "Built BVH for Mesh #{} (name: \"{}\")",
            mesh_component.mesh->GetID().Value(),
            mesh_component.mesh->GetName());

        GetEntityManager().RemoveTag<EntityTag::UPDATE_BVH>(entity);
    }
    else
    {
        HYP_LOG(ECS, Warning, "Failed to calculate BVH for Mesh #{} (name: \"{}\")",
            mesh_component.mesh->GetID().Value(),
            mesh_component.mesh->GetName());
    }
}

void BVHUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void BVHUpdaterSystem::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updated_entities;

    for (auto [entity, bvh_component, mesh_component, transform_component, _] : GetEntityManager().GetEntitySet<BVHComponent, MeshComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_BVH>>().GetScopedView(GetComponentInfos()))
    {
        if (!mesh_component.mesh.IsValid())
        {
            continue;
        }

        if (mesh_component.mesh->BuildBVH(bvh_component.bvh, /* max_depth */ 3))
        {
            HYP_LOG(ECS, Info, "Built BVH for Mesh #{} (name: \"{}\")",
                mesh_component.mesh->GetID(),
                mesh_component.mesh->GetName());

            updated_entities.Insert(entity->WeakHandleFromThis());
        }
        else
        {
            HYP_LOG(ECS, Warning, "Failed to calculate BVH for Mesh #{} (name: \"{}\")",
                mesh_component.mesh->GetID(),
                mesh_component.mesh->GetName());
        }
    }

    if (updated_entities.Any())
    {
        AfterProcess([this, updated_entities = std::move(updated_entities)]()
            {
                for (const WeakHandle<Entity>& entity_weak : updated_entities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_BVH>(entity_weak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
