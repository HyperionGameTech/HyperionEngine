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

    if (visibility_state_component.octant_id != OctantID::invalid) {
        return;
    }

    visibility_state_component.visibility_state = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity);

    if (bounding_box_component.world_aabb.IsValid()) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

        if (insert_result.first) {
            AssertThrowMsg(insert_result.second != OctantID::invalid, "Invalid octant ID returned from Insert()");

            visibility_state_component.octant_id = insert_result.second;

            if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
                visibility_state_component.visibility_state = octant->GetVisibilityState().Get();
            }

            DebugLog(LogType::Debug, "Inserted entity %u into octree, inserted at %u, %u\n", entity.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
        } else {
            DebugLog(LogType::Warn, "Failed to insert entity %u into octree: %s\n", entity.Value(), insert_result.first.message);
        }
    } else {
        DebugLog(LogType::Warn, "Entity %u has invalid bounding box, skipping octree insertion\n", entity.Value());
    }

    visibility_state_component.last_aabb_hash = bounding_box_component.world_aabb.GetHashCode();
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity);
    visibility_state_component.visibility_state = nullptr;

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

    for (auto [entity_id, visibility_state_component, bounding_box_component] : entity_manager.GetEntitySet<VisibilityStateComponent, BoundingBoxComponent>()) {
        bool needs_octree_update = false;

        const bool visibility_state_invalidated = visibility_state_component.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        if (visibility_state_invalidated) {
            DebugLog(
                LogType::Debug,
                "VisibilityStateUpdaterSystem::Process(%u): visibility state invalidated\n",
                entity_id.Value()
            );
        }

        const HashCode aabb_hash_code = bounding_box_component.world_aabb.GetHashCode();

        needs_octree_update |= (aabb_hash_code != visibility_state_component.last_aabb_hash);
        needs_octree_update |= visibility_state_invalidated;

        visibility_state_component.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        if (needs_octree_update) {
            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the octree to be updated
            const bool force_entry_invalidation = visibility_state_invalidated;

            auto update_result = octree.Update(entity_id, bounding_box_component.world_aabb, force_entry_invalidation);

            if (!update_result.first) {
                // DebugLog(LogType::Warn, "Failed to update entity %u in octree: %s\n", entity_id.Value(), update_result.first.message);

                continue;
            }

            AssertThrowMsg(update_result.second != OctantID::invalid, "Invalid octant ID returned from Update()");

            visibility_state_component.octant_id = update_result.second;
            visibility_state_component.last_aabb_hash = aabb_hash_code;
        }

        if (visibility_state_component.octant_id != OctantID::invalid) {
            Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id);

            if (octant) {
                visibility_state_component.visibility_state = octant->GetVisibilityState().Get();

                continue;
            }
        }

        visibility_state_component.visibility_state = nullptr;
    }
}

} // namespace hyperion::v2
