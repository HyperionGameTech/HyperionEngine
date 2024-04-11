#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WorldAABBUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity);
    TransformComponent &transform_component = entity_manager.GetComponent<TransformComponent>(entity);

    bounding_box_component.world_aabb = BoundingBox::Empty();

    if (bounding_box_component.local_aabb.IsValid()) {
        for (const Vec3f &corner : bounding_box_component.local_aabb.GetCorners()) {
            bounding_box_component.world_aabb.Extend(transform_component.transform.GetMatrix() * corner);
        }
    }

    bounding_box_component.transform_hash_code = transform_component.transform.GetHashCode();

    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);
    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void WorldAABBUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);
}

void WorldAABBUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, bounding_box_component, transform_component, mesh_component] : entity_manager.GetEntitySet<BoundingBoxComponent, TransformComponent, MeshComponent>()) {
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
        
            // Tell the renderer that the entity's world AABB has changed.
            // This will cause the renderer to update the entity's world AABB in the GPU.
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion::v2