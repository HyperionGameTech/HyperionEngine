#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WorldAABBUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, bounding_box_component, transform_component, mesh_component] : entity_manager.GetEntitySet<BoundingBoxComponent, TransformComponent, MeshComponent>()) {
        const BoundingBox local_aabb = bounding_box_component.local_aabb;
        BoundingBox world_aabb = bounding_box_component.world_aabb;

        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (transform_hash_code != bounding_box_component.transform_hash_code) {
            world_aabb = BoundingBox::empty;

            if (!local_aabb.Empty()) {
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