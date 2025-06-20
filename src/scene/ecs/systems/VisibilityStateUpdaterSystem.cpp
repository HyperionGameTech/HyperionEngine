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

void VisibilityStateUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    VisibilityStateComponent& visibility_state_component = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    GetEntityManager().AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);

    if (visibility_state_component.octant_id != OctantID::Invalid())
    {
        return;
    }

    visibility_state_component.visibility_state = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent& bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

    if (insert_result.first)
    {
        AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

        visibility_state_component.octant_id = insert_result.second;
        visibility_state_component.visibility_state = nullptr;

        if (Octree* octant = octree.GetChildOctant(visibility_state_component.octant_id))
        {
            visibility_state_component.visibility_state = &octant->GetVisibilityState();
        }

        // HYP_LOG(Octree, Debug, "Inserted entity #{} into octree, inserted at {}, {}", entity.GetID().Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());

        // GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }
    else
    {
        HYP_LOG(Octree, Warning, "Failed to insert entity #{} into octree: {}", entity->GetID(), insert_result.first.message);
    }
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    VisibilityStateComponent& visibility_state_component = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::Result remove_result = octree.Remove(entity);

    if (!remove_result)
    {
        HYP_LOG(Octree, Warning, "Failed to remove Entity #{} from octree: {}", entity->GetID(), remove_result.message);
    }

    visibility_state_component.octant_id = OctantID::Invalid();
    visibility_state_component.visibility_state = nullptr;
}

void VisibilityStateUpdaterSystem::Process(float delta)
{
    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    HashSet<WeakHandle<Entity>> updated_entities;

    const auto update_visbility_state = [&octree, &updated_entities](Entity* entity, VisibilityStateComponent& visibility_state_component, BoundingBoxComponent& bounding_box_component)
    {
        bool needs_octree_update = false;

        const bool visibility_state_invalidated = visibility_state_component.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        needs_octree_update |= visibility_state_invalidated;

        visibility_state_component.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        // if entity is not in the octree, try to insert it
        if (visibility_state_component.octant_id == OctantID::Invalid())
        {
            visibility_state_component.visibility_state = nullptr;

            if (!bounding_box_component.world_aabb.IsValid())
            {
                return;
            }

            const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

            if (insert_result.first)
            {
                AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

                visibility_state_component.octant_id = insert_result.second;

                if (Octree* octant = octree.GetChildOctant(visibility_state_component.octant_id))
                {
                    visibility_state_component.visibility_state = &octant->GetVisibilityState();
                }
            }

            return;
        }

        if (needs_octree_update)
        {
            visibility_state_component.visibility_state = nullptr;

            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the entire octree to be updated.
            const bool force_entry_invalidation = visibility_state_invalidated;

            const Octree::InsertResult update_result = octree.Update(entity, bounding_box_component.world_aabb, force_entry_invalidation);

            visibility_state_component.octant_id = update_result.second;

            if (!update_result.first)
            {
                HYP_LOG(Octree, Warning, "Failed to update entity {} in octree: {}", entity->GetID(), update_result.first.message);

                return;
            }

            AssertThrowMsg(update_result.second != OctantID::Invalid(), "Invalid octant ID returned from Update()");
        }

        if (visibility_state_component.octant_id != OctantID::Invalid())
        {
            visibility_state_component.visibility_state = nullptr;

            if (Octree* octant = octree.GetChildOctant(visibility_state_component.octant_id))
            {
                visibility_state_component.visibility_state = &octant->GetVisibilityState();
            }
        }

        updated_entities.Insert(entity->WeakHandleFromThis());
    };

    for (auto [entity, visibility_state_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<VisibilityStateComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>>().GetScopedView(GetComponentInfos()))
    {
        update_visbility_state(entity, visibility_state_component, bounding_box_component);
    }

    if (updated_entities.Any())
    {
        AfterProcess([this, updated_entities = std::move(updated_entities)]()
            {
                for (const WeakHandle<Entity>& entity_weak : updated_entities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity_weak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
