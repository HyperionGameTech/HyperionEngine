/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion {

Name SystemBase::GetName() const
{
    return InstanceClass()->GetName();
}

TypeID SystemBase::GetTypeID() const
{
    return InstanceClass()->GetTypeID();
}

Scene* SystemBase::GetScene() const
{
    return GetEntityManager().GetScene();
}

World* SystemBase::GetWorld() const
{
    return GetEntityManager().GetWorld();
}

void SystemBase::SetWorld(World* world)
{
    OnWorldChanged(world, GetWorld());
}

} // namespace hyperion