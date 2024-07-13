/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion {

void WorldAABBUpdaterSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    bounding_box_component.world_aabb = BoundingBox::Empty();

    if (bounding_box_component.local_aabb.IsValid()) {
        for (const Vec3f &corner : bounding_box_component.local_aabb.GetCorners()) {
            bounding_box_component.world_aabb.Extend(transform_component.transform.GetMatrix() * corner);
        }
    }

    bounding_box_component.transform_hash_code = transform_component.transform.GetHashCode();

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void WorldAABBUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, bounding_box_component, transform_component, mesh_component] : GetEntityManager().GetEntitySet<BoundingBoxComponent, TransformComponent, MeshComponent>()) {
        const BoundingBox local_aabb = bounding_box_component.local_aabb;
        BoundingBox world_aabb = bounding_box_component.world_aabb;

        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (transform_hash_code != bounding_box_component.transform_hash_code) {
            world_aabb = BoundingBox::Empty();

            if (local_aabb.IsValid()) {
                for (const Vec3f &corner : local_aabb.GetCorners()) {
                    world_aabb.Extend(transform_component.transform.GetMatrix() * corner);
                }
            }

            bounding_box_component.local_aabb = local_aabb;
            bounding_box_component.world_aabb = world_aabb;
            bounding_box_component.transform_hash_code = transform_hash_code;
        
            // Update the render proxy
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion