#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/Scene.hpp>
#include <scene/Octree.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void VisibilityStateUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    DebugLog(
        LogType::Debug,
        "VisibilityStateUpdaterSystem::OnEntityAdded(%u)\n",
        entity.Value()
    );

    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity);
    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity);

    if (visibility_state_component.octant_id != OctantID::invalid) {
        return;
    }

    if (bounding_box_component.world_aabb.IsFinite()) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::InsertResult insert_result = octree.Insert(
            entity,
            bounding_box_component.world_aabb,
            false
        );

        if (insert_result.first) {
            AssertThrowMsg(insert_result.second != OctantID::invalid, "Invalid octant ID returned from Insert()");

            visibility_state_component.octant_id = insert_result.second;

            DebugLog(LogType::Debug, "Inserted entity %u into octree, inserted at %u, %u\n", entity.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
        } else {
            DebugLog(LogType::Warn, "Failed to insert entity %u into octree: %s\n", entity.Value(), insert_result.first.message);
        }
    }

    visibility_state_component.last_aabb_hash = bounding_box_component.world_aabb.GetHashCode();
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity);

    if (visibility_state_component.octant_id != OctantID::invalid) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::Result remove_result = octree.Remove(entity, false);

        if (!remove_result) {
            DebugLog(LogType::Warn, "Failed to remove entity %u from octree: %s\n", entity.Value(), remove_result.message);
        }

        visibility_state_component.octant_id = OctantID::invalid;
    }
}

void VisibilityStateUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Octree &octree = entity_manager.GetScene()->GetOctree();

    const uint8 visibility_cursor = octree.LoadVisibilityCursor();

    for (auto [entity_id, visibility_state_component, bounding_box_component] : entity_manager.GetEntitySet<VisibilityStateComponent, BoundingBoxComponent>()) {
        bool needs_octree_update = false;

        const HashCode aabb_hash_code = bounding_box_component.world_aabb.GetHashCode();
        needs_octree_update |= (aabb_hash_code != visibility_state_component.last_aabb_hash);

        if (needs_octree_update) {
            auto update_result = octree.Update(entity_id, bounding_box_component.world_aabb, false);

            if (!update_result.first) {
                DebugLog(LogType::Warn, "Failed to update entity %u in octree: %s\n", entity_id.Value(), update_result.first.message);

                continue;
            }

            AssertThrowMsg(update_result.second != OctantID::invalid, "Invalid octant ID returned from Update()");

            visibility_state_component.octant_id = update_result.second;
            visibility_state_component.last_aabb_hash = aabb_hash_code;
        }

        if (visibility_state_component.octant_id == OctantID::invalid) {
            continue;
        }

        Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id);

        if (octant) {
            const VisibilityState &octant_visibility_state = octant->GetVisibilityState();

            visibility_state_component.visibility_state.snapshots[visibility_cursor] = octant_visibility_state.snapshots[visibility_cursor];
        }
    }

    // Rebuild any octants that have had structural changes
    octree.PerformUpdates();
}

} // namespace hyperion::v2
