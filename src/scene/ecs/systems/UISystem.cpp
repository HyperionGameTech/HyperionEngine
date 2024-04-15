/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/UISystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void UISystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

}

void UISystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);
}

void UISystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    // do nothing
}

} // namespace hyperion::v2
