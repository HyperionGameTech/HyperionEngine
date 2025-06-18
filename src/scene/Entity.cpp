/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

Entity::Entity()
    : m_world(nullptr),
      m_scene(nullptr)
{
}

Entity::~Entity()
{
    const ID<Entity> id = GetID();

    if (!id.IsValid())
    {
        return;
    }

    // Keep a WeakHandle of Entity so the ID doesn't get reused while we're using it
    EntityManager::GetEntityToEntityManagerMap().PerformActionWithEntity_FireAndForget(id, [weak_this = WeakHandleFromThis()](EntityManager* entity_manager, ID<Entity> id)
        {
            HYP_NAMED_SCOPE("Remove Entity from EntityManager (task)");

            HYP_LOG(ECS, Debug, "Removing Entity {} from entity manager", id.Value());

            AssertThrow(entity_manager->HasEntity(id));

            if (!entity_manager->RemoveEntity(id))
            {
                HYP_LOG(ECS, Error, "Failed to remove Entity {} from EntityManager", id.Value());
            }
        });
}

void Entity::Init()
{
    SetReady(true);
}

} // namespace hyperion