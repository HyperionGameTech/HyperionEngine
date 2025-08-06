/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Entity);

void WorldAABBUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (ProcessEntity(entity, GetEntityManager().GetComponent<BoundingBoxComponent>(entity), GetEntityManager().GetComponent<TransformComponent>(entity)))
    {
        GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE, EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entity);

        GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entity);
    }
}

void WorldAABBUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updatedEntities;

    for (auto [entity, boundingBoxComponent, transformComponent, _] : GetEntityManager().GetEntitySet<BoundingBoxComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_AABB>>().GetScopedView(GetComponentInfos()))
    {
        if (ProcessEntity(entity, boundingBoxComponent, transformComponent))
        {
            updatedEntities.Insert(entity->WeakHandleFromThis());
        }
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE, EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entityWeak.GetUnsafe());

                    GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entityWeak.GetUnsafe());
                }
            });
    }
}

bool WorldAABBUpdaterSystem::ProcessEntity(Entity* entity, BoundingBoxComponent& boundingBoxComponent, TransformComponent& transformComponent)
{
    const BoundingBox localAabb = boundingBoxComponent.localAabb;
    BoundingBox worldAabb = boundingBoxComponent.worldAabb;

    worldAabb = BoundingBox::Empty();

    if (localAabb.IsValid())
    {
        for (const Vec3f& corner : localAabb.GetCorners())
        {
            worldAabb = worldAabb.Union(transformComponent.transform.GetMatrix() * corner);
        }
    }

    boundingBoxComponent.localAabb = localAabb;
    boundingBoxComponent.worldAabb = worldAabb;

    return true;
}

} // namespace hyperion