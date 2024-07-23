/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <core/Handle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT void World_AddScene(World *world, ManagedHandle scene_handle)
{
    Handle<Scene> scene = CreateHandleFromManagedHandle<Scene>(scene_handle);

    world->AddScene(std::move(scene));
}

HYP_EXPORT hyperion::uint32 World_GetID(World *world)
{
    return world->GetID().Value();
}

HYP_EXPORT SubsystemBase *World_GetSubsystem(World *world, hyperion::uint32 type_id)
{
    return world->GetSubsystem(TypeID { type_id });
}

} // extern "C"