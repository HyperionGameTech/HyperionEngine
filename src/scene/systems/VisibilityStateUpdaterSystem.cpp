/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/SceneOctree.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

bool VisibilityStateUpdaterSystem::ShouldCreateForScene(Scene* scene) const
{
    return !(scene->GetFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void VisibilityStateUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    VisibilityStateComponent& visibilityStateComponent = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    if (visibilityStateComponent.octantId != OctantId::Invalid())
    {
        return;
    }

    GetEntityManager().AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);

    visibilityStateComponent.visibilityState = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent& boundingBoxComponent = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    SceneOctree& octree = GetEntityManager().GetScene()->GetOctree();

    const SceneOctree::Result insertResult = octree.Insert(entity, boundingBoxComponent.worldAabb);

    if (insertResult.HasValue())
    {
        OctantId octantId = insertResult.GetValue();
        Assert(octantId != OctantId::Invalid(), "Invalid octant Id returned from Insert()");

        visibilityStateComponent.octantId = octantId;
        visibilityStateComponent.visibilityState = nullptr;

        if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
        {
            visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
        }

        // HYP_LOG(Octree, Debug, "Inserted entity #{} into octree, inserted at {}, {}", entity.Id().Value(), visibilityStateComponent.octantId.GetIndex(), visibilityStateComponent.octantId.GetDepth());

        GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }
    else
    {
        HYP_LOG(Scene, Warning, "Failed to insert entity #{} into octree: {}", entity->Id(), insertResult.GetError().GetMessage());
    }
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    VisibilityStateComponent& visibilityStateComponent = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    SceneOctree& octree = GetEntityManager().GetScene()->GetOctree();

    const SceneOctree::Result removeResult = octree.Remove(entity);

    if (removeResult.HasError())
    {
        HYP_LOG(Scene, Warning, "Failed to remove Entity #{} from octree: {}", entity->Id(), removeResult.GetError().GetMessage());
    }

    visibilityStateComponent.octantId = OctantId::Invalid();
    visibilityStateComponent.visibilityState = nullptr;
}

void VisibilityStateUpdaterSystem::Process(float delta)
{
    SceneOctree& octree = GetEntityManager().GetScene()->GetOctree();

    HashSet<WeakHandle<Entity>> updatedEntities;

    const auto updateVisbilityState = [&octree, &updatedEntities](Entity* entity, VisibilityStateComponent& visibilityStateComponent, BoundingBoxComponent& boundingBoxComponent)
    {
        const bool visibilityStateInvalidated = visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        visibilityStateComponent.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        // if entity is not in the octree, try to insert it
        if (visibilityStateComponent.octantId == OctantId::Invalid())
        {
            visibilityStateComponent.visibilityState = nullptr;

            if (!boundingBoxComponent.worldAabb.IsValid())
            {
                return;
            }

            const SceneOctree::Result insertResult = octree.Insert(entity, boundingBoxComponent.worldAabb);

            if (insertResult.HasValue())
            {
                AssertDebug(insertResult.GetValue() != OctantId::Invalid(), "Invalid OctantId returned from Insert()");

                visibilityStateComponent.octantId = insertResult.GetValue();

                if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
                {
                    visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
                }

                updatedEntities.Insert(entity->WeakHandleFromThis());
            }

            return;
        }

        visibilityStateComponent.visibilityState = nullptr;

        // force entry invalidation if the bounding box is not finite,
        // so directional lights changing cause the entire octree to be updated.
        const bool forceEntryInvalidation = visibilityStateInvalidated;

        const SceneOctree::Result updateResult = octree.Update(entity, boundingBoxComponent.worldAabb, forceEntryInvalidation);

        if (updateResult.HasError())
        {
            visibilityStateComponent.octantId = OctantId::Invalid();

            HYP_LOG(Scene, Warning, "Failed to update entity {} in octree: {}", entity->Id(), updateResult.GetError().GetMessage());

            return;
        }

        visibilityStateComponent.octantId = updateResult.GetValue();

        if (visibilityStateComponent.octantId.IsInvalid())
        {
            AssertDebug(false, "Invalid OctantId returned from Update()");

            return;
        }

        visibilityStateComponent.visibilityState = nullptr;

        if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
        {
            visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
        }

        updatedEntities.Insert(entity->WeakHandleFromThis());
    };

    for (auto [entity, visibilityStateComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<VisibilityStateComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>>().GetScopedView(GetComponentInfos()))
    {
        updateVisbilityState(entity, visibilityStateComponent, boundingBoxComponent);
    }

    if (updatedEntities.Any())
    {
#ifdef HYP_DEBUG_MODE
        if (updatedEntities.Size() >= 128)
        {
            HYP_LOG(Scene, Warning, "Updating visibility states for a lot of entities ({})! This will have a performance impact if it happens frequently."
                                    "\n\tMaybe the Scene's octree should have a different bounding size or be broken into multiple Scenes."
                                    "\n\tScene name: {}, flags: {}",
                updatedEntities.Size(), GetScene()->GetName(), uint32(GetScene()->GetFlags()));
        }
#endif

        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entityWeak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
