/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

namespace hyperion {

Scene *SystemBase::GetScene() const
{
    return m_entity_manager.GetScene();
}

World *SystemBase::GetWorld() const
{
    return m_entity_manager.GetScene()->GetWorld();
}

} // namespace hyperion