#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Octree.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void VisibilityStateUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Octree &octree = g_engine->GetWorld()->GetOctree();

    const UInt8 visibility_cursor = octree.LoadVisibilityCursor();

    for (auto [entity_id, visibility_state_component, bounding_box_component] : entity_manager.GetEntitySet<VisibilityStateComponent, BoundingBoxComponent>()) {
        Bool needs_octree_update = false;
        
        if (visibility_state_component.octant_id == OctantID::invalid) {
            const Octree::InsertResult insert_result = octree.Insert(
                entity_id,
                bounding_box_component.aabb
            );

            if (insert_result.first) {
                visibility_state_component.octant_id = insert_result.second;

                DebugLog(LogType::Debug, "Inserted entity %u into octree, inserted at %u\n", entity_id.Value(), visibility_state_component.octant_id.GetIndex());
            } else {
                DebugLog(LogType::Warn, "Failed to insert entity %u into octree: %s\n", entity_id.Value(), insert_result.first.message);

                continue;
            }

            needs_octree_update = true;
        }
        
        const HashCode aabb_hash_code = bounding_box_component.aabb.GetHashCode();
        needs_octree_update |= (aabb_hash_code != visibility_state_component.last_aabb_hash);

        if (needs_octree_update) {
            octree.Update(entity_id, bounding_box_component.aabb);

            visibility_state_component.last_aabb_hash = aabb_hash_code;
        }
        
        Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id);

        if (octant) {
            const VisibilityState &octant_visibility_state = octant->GetVisibilityState();

            visibility_state_component.visibility_state.snapshots[visibility_cursor] = octant_visibility_state.snapshots[visibility_cursor];
        }
    }
}

} // namespace hyperion::v2