/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BVHUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/Mesh.hpp>

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

    if (mesh_component.mesh->BuildBVH(transform_component.transform.GetMatrix(), bvh_component.bvh, /* max_depth */ 3)) {
        HYP_LOG(BVH, Info, "Built BVH for Mesh #{} (name: \"{}\")",
            mesh_component.mesh->GetID().Value(),
            mesh_component.mesh->GetName());

        bvh_component.transform_hash_code = transform_component.transform.GetHashCode();
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
    for (auto [entity_id, bvh_component, mesh_component, transform_component] : GetEntityManager().GetEntitySet<BVHComponent, MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos())) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (!bvh_component.bvh.IsValid() || transform_hash_code != bvh_component.transform_hash_code) {
            if (!mesh_component.mesh.IsValid()) {
                continue;
            }

            if (mesh_component.mesh->BuildBVH(transform_component.transform.GetMatrix(), bvh_component.bvh, /* max_depth */ 3)) {
                HYP_LOG(BVH, Info, "Built BVH for Mesh #{} (name: \"{}\")",
                    mesh_component.mesh->GetID().Value(),
                    mesh_component.mesh->GetName());

                bvh_component.transform_hash_code = transform_hash_code;
            } else {
                HYP_LOG(BVH, Warning, "Failed to calculate BVH for Mesh #{} (name: \"{}\")",
                    mesh_component.mesh->GetID().Value(),
                    mesh_component.mesh->GetName());
            }
        }
    }
}

} // namespace hyperion
