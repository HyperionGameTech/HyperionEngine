/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

Entity::Entity() = default;

Entity::~Entity()
{
    const ID<Entity> id = GetID();

    if (!id.IsValid()) {
        return;
    }

    // Keep a WeakHandle of Entity so the ID doesn't get reused while we're using it
    EntityManager::GetEntityToEntityManagerMap().PerformActionWithEntity_FireAndForget(id, [weak_this = WeakHandleFromThis()](EntityManager *entity_manager, ID<Entity> id)
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (task)");

        entity_manager->RemoveEntity(id);
    });
}

} // namespace hyperion