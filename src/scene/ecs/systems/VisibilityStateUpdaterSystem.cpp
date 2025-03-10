/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/LightComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/Octree.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

void VisibilityStateUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    VisibilityStateComponent &visibility_state_component = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    GetEntityManager().AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);

    if (visibility_state_component.octant_id != OctantID::Invalid()) {
        HYP_LOG(Octree, Debug, "Entity #{} already has an octant ID assigned", entity.GetID().Value());

        return;
    }

    visibility_state_component.visibility_state = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    Octree &octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

    if (insert_result.first) {
        AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

        visibility_state_component.octant_id = insert_result.second;
        visibility_state_component.visibility_state = nullptr;

        if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
            visibility_state_component.visibility_state = &octant->GetVisibilityState();
        }

        HYP_LOG(Octree, Debug, "Inserted entity #{} into octree, inserted at {}, {}", entity.GetID().Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());

        //GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    } else {
        HYP_LOG(Octree, Warning, "Failed to insert entity #{} into octree: {}", entity.GetID().Value(), insert_result.first.message);
    }
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    VisibilityStateComponent &visibility_state_component = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    Octree &octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::Result remove_result = octree.Remove(entity);

    if (!remove_result) {
        HYP_LOG(Octree, Warning, "Failed to remove entity {} from octree: {}", entity.Value(), remove_result.message);
    }

    visibility_state_component.octant_id = OctantID::Invalid();
    visibility_state_component.visibility_state = nullptr;
}

void VisibilityStateUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    Octree &octree = GetEntityManager().GetScene()->GetOctree();

    HashSet<ID<Entity>> updated_entity_ids;

    const auto UpdateVisbilityState = [&octree, &updated_entity_ids](ID<Entity> entity_id, VisibilityStateComponent &visibility_state_component, BoundingBoxComponent &bounding_box_component)
    {
        bool needs_octree_update = false;

        const bool visibility_state_invalidated = visibility_state_component.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        needs_octree_update |= visibility_state_invalidated;

        visibility_state_component.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        // if entity is not in the octree, try to insert it
        if (visibility_state_component.octant_id == OctantID::Invalid()) {
            visibility_state_component.visibility_state = nullptr;

            if (!bounding_box_component.world_aabb.IsValid()) {
                return;
            }

            Handle<Entity> entity { entity_id };
            AssertThrow(entity.IsValid());

            const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

            if (insert_result.first) {
                AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

                visibility_state_component.octant_id = insert_result.second;

                if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
                    visibility_state_component.visibility_state = &octant->GetVisibilityState();
                }

                HYP_LOG(Octree, Debug, "Inserted entity {} into octree, inserted at {}, {}", entity_id.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
            }

            return;
        }

        if (needs_octree_update) {
            visibility_state_component.visibility_state = nullptr;

            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the entire octree to be updated.
            const bool force_entry_invalidation = visibility_state_invalidated;

            const Octree::InsertResult update_result = octree.Update(entity_id, bounding_box_component.world_aabb, force_entry_invalidation);

            visibility_state_component.octant_id = update_result.second;

            if (!update_result.first) {
                HYP_LOG(Octree, Warning, "Failed to update entity {} in octree: {}", entity_id.Value(), update_result.first.message);

                return;
            }

            AssertThrowMsg(update_result.second != OctantID::Invalid(), "Invalid octant ID returned from Update()");
        }

        if (visibility_state_component.octant_id != OctantID::Invalid()) {
            visibility_state_component.visibility_state = nullptr;

            if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
                visibility_state_component.visibility_state = &octant->GetVisibilityState();
            }
        }

        updated_entity_ids.Insert(entity_id);
    };

    for (auto [entity_id, visibility_state_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<VisibilityStateComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>>().GetScopedView(GetComponentInfos())) {
        UpdateVisbilityState(entity_id, visibility_state_component, bounding_box_component);
    }

    if (updated_entity_ids.Any()) {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
        {
            for (const ID<Entity> &entity_id : entity_ids) {
                //GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity_id);
            }
        });
    }
}

} // namespace hyperion
