/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/logging/Logger.hpp>

#include <WorldAABBUpdaterSystem.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Entity);

void WorldAABBUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (ProcessEntity(entity, entity->GetEntityManager()->GetComponent<BoundingBoxComponent>(entity), entity->GetEntityManager()->GetComponent<TransformComponent>(entity)))
    {
        entity->GetEntityManager()->AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }

    entity->GetEntityManager()->RemoveTag<EntityTag::UPDATE_AABB>(entity);
}

void WorldAABBUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        HashMap<WeakHandle<Entity>, bool> updatedEntities;

        for (auto [entity, boundingBoxComponent, transformComponent, _] : scene->GetEntityManager()->GetEntitySet<BoundingBoxComponent, TransformComponent, TagComponent<EntityTag::UPDATE_AABB>>().GetScopedView(GetComponentInfos()))
        {
            const bool wasWorldAabbChanged = ProcessEntity(entity, boundingBoxComponent, transformComponent);

            updatedEntities[MakeWeakRef(entity)] = wasWorldAabbChanged;
        }

        if (updatedEntities.Any())
        {
            AfterProcess([this, scene, updatedEntities = std::move(updatedEntities)]()
                {
                    for (const auto& [entityWeak, wasWorldAabbChanged] : updatedEntities)
                    {
                        Entity* entity = entityWeak.GetUnsafe(); // don't use ptr so it's fine to use GetUnsafe()

                        if (wasWorldAabbChanged)
                        {
                            scene->GetEntityManager()->AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE>(entity);
                        }

                        scene->GetEntityManager()->RemoveTag<EntityTag::UPDATE_AABB>(entity);
                    }
                });
        }
    }
}

//! Return true on change
bool WorldAABBUpdaterSystem::ProcessEntity(Entity* entity, BoundingBoxComponent& boundingBoxComponent, TransformComponent& transformComponent)
{
    const BoundingBox prevWorldAabb = boundingBoxComponent.worldAabb;
    const BoundingBox& localAabb = boundingBoxComponent.localAabb;
    BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

    worldAabb = BoundingBox::Empty();

    const Mat4f transformMatrix = transformComponent.transform.GetMatrix();

    if (localAabb.IsValid())
    {
        for (const Vec3f& corner : localAabb.GetCorners())
        {
            worldAabb = worldAabb.Union(transformMatrix * corner);
        }
    }

    if (prevWorldAabb == worldAabb)
    {
        // no change
        return false;
    }

    boundingBoxComponent.worldAabb = worldAabb;

    return true;
}

} // namespace hyperion
