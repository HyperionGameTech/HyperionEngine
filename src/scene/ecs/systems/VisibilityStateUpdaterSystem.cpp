#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/Octree.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void VisibilityStateUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity_id)
{
    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity_id);
    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity_id);

    if (visibility_state_component.octant_id != OctantID::invalid) {
        return;
    }

    Octree &octree = entity_manager.GetScene()->GetOctree();

    const Octree::InsertResult insert_result = octree.Insert(
        entity_id,
        bounding_box_component.world_aabb
    );

    if (insert_result.first) {
        AssertThrowMsg(insert_result.second != OctantID::invalid, "Invalid octant ID returned from Insert()");

        visibility_state_component.octant_id = insert_result.second;

        DebugLog(LogType::Debug, "Inserted entity %u into octree, inserted at %u, %u\n", entity_id.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
    } else {
        DebugLog(LogType::Warn, "Failed to insert entity %u into octree: %s\n", entity_id.Value(), insert_result.first.message);
    }
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity_id)
{
    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity_id);

    if (visibility_state_component.octant_id != OctantID::invalid) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::Result remove_result = octree.Remove(entity_id);

        if (!remove_result) {
            DebugLog(LogType::Warn, "Failed to remove entity %u from octree: %s\n", entity_id.Value(), remove_result.message);
        }

        visibility_state_component.octant_id = OctantID::invalid;
    }
}

void VisibilityStateUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Octree &octree = entity_manager.GetScene()->GetOctree();

    const UInt8 visibility_cursor = octree.LoadVisibilityCursor();

    for (auto [entity_id, visibility_state_component, bounding_box_component] : entity_manager.GetEntitySet<VisibilityStateComponent, BoundingBoxComponent>()) {
        Bool needs_octree_update = false;
        
        if (visibility_state_component.octant_id == OctantID::invalid) {
            

            needs_octree_update = true;
        }
        
        const HashCode aabb_hash_code = bounding_box_component.world_aabb.GetHashCode();
        needs_octree_update |= (aabb_hash_code != visibility_state_component.last_aabb_hash);

        if (needs_octree_update) {
            auto update_result = octree.Update(entity_id, bounding_box_component.world_aabb);

            if (!update_result.first) {
                DebugLog(LogType::Warn, "Failed to update entity %u in octree: %s\n", entity_id.Value(), update_result.first.message);

                continue;
            }

            AssertThrowMsg(update_result.second != OctantID::invalid, "Invalid octant ID returned from Update()");

            visibility_state_component.octant_id = update_result.second;
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