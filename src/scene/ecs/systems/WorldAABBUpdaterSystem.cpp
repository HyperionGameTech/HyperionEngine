#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WorldAABBUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, bounding_box_component, transform_component, mesh_component] : entity_manager.GetEntitySet<BoundingBoxComponent, TransformComponent, MeshComponent>()) {
        const BoundingBox local_aabb = mesh_component.mesh.IsValid()
            ? mesh_component.mesh->GetAABB()
            : BoundingBox::empty;

        if (local_aabb != bounding_box_component.local_aabb) {
            BoundingBox world_aabb = BoundingBox::empty;

            if (!local_aabb.Empty()) {
                for (const Vec3f &corner : world_aabb.GetCorners()) {
                    world_aabb.Extend(transform_component.transform.GetMatrix() * corner);
                }
            }

            bounding_box_component.local_aabb = local_aabb;
            bounding_box_component.world_aabb = world_aabb;
        
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion::v2