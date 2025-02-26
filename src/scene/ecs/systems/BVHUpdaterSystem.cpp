/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BVHUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Mesh.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(ECS);
HYP_DEFINE_LOG_SUBCHANNEL(BVH, ECS);

void BVHUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    BVHComponent &bvh_component = GetEntityManager().GetComponent<BVHComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid()) {
        bvh_component.bvh = BVHNode();

        return;
    }

    if (mesh_component.mesh->BuildBVH(bvh_component.bvh, /* max_depth */ 3)) {
        HYP_LOG(BVH, Info, "Built BVH for Mesh #{} (name: \"{}\")",
            mesh_component.mesh->GetID().Value(),
            mesh_component.mesh->GetName());
            
        GetEntityManager().RemoveTag<EntityTag::UPDATE_BVH>(entity);
    } else {
        HYP_LOG(BVH, Warning, "Failed to calculate BVH for Mesh #{} (name: \"{}\")",
            mesh_component.mesh->GetID().Value(),
            mesh_component.mesh->GetName());
    }
}

void BVHUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void BVHUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    HashSet<ID<Entity>> updated_entity_ids;

    for (auto [entity_id, bvh_component, mesh_component, transform_component, _] : GetEntityManager().GetEntitySet<BVHComponent, MeshComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_BVH>>().GetScopedView(GetComponentInfos())) {
        if (!mesh_component.mesh.IsValid()) {
            continue;
        }

        if (mesh_component.mesh->BuildBVH(bvh_component.bvh, /* max_depth */ 3)) {
            HYP_LOG(BVH, Info, "Built BVH for Mesh #{} (name: \"{}\")",
                mesh_component.mesh->GetID().Value(),
                mesh_component.mesh->GetName());

            updated_entity_ids.Insert(entity_id);
        } else {
            HYP_LOG(BVH, Warning, "Failed to calculate BVH for Mesh #{} (name: \"{}\")",
                mesh_component.mesh->GetID().Value(),
                mesh_component.mesh->GetName());
        }
    }

    if (updated_entity_ids.Any()) {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
        {
            for (const ID<Entity> &entity_id : entity_ids) {
                GetEntityManager().RemoveTag<EntityTag::UPDATE_BVH>(entity_id);
            }
        });
    }
}

} // namespace hyperion
