/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(ECS);

void WorldAABBUpdaterSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    if (ProcessEntity(entity, GetEntityManager().GetComponent<BoundingBoxComponent>(entity), GetEntityManager().GetComponent<TransformComponent>(entity)))
    {
        GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE, EntityTag::UPDATE_ENV_PROBE_TRANSFORM, EntityTag::UPDATE_BLAS>(entity);

        GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entity);
    }
}

void WorldAABBUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    HashSet<ID<Entity>> updated_entity_ids;

    for (auto [entity_id, bounding_box_component, transform_component, _] : GetEntityManager().GetEntitySet<BoundingBoxComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_AABB>>().GetScopedView(GetComponentInfos()))
    {
        if (ProcessEntity(entity_id, bounding_box_component, transform_component))
        {
            updated_entity_ids.Insert(entity_id);
        }
    }

    if (updated_entity_ids.Any())
    {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity>& entity_id : entity_ids)
                {
                    GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE, EntityTag::UPDATE_ENV_PROBE_TRANSFORM, EntityTag::UPDATE_BLAS>(entity_id);

                    GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entity_id);
                }
            });
    }
}

bool WorldAABBUpdaterSystem::ProcessEntity(ID<Entity>, BoundingBoxComponent& bounding_box_component, TransformComponent& transform_component)
{
    const BoundingBox local_aabb = bounding_box_component.local_aabb;
    BoundingBox world_aabb = bounding_box_component.world_aabb;

    world_aabb = BoundingBox::Empty();

    if (local_aabb.IsValid())
    {
        for (const Vec3f& corner : local_aabb.GetCorners())
        {
            world_aabb = world_aabb.Union(transform_component.transform.GetMatrix() * corner);
        }
    }

    bounding_box_component.local_aabb = local_aabb;
    bounding_box_component.world_aabb = world_aabb;

    return true;
}

} // namespace hyperion