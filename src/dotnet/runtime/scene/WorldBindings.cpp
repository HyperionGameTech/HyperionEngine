#include <dotnet/runtime/ManagedHandle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <core/Handle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

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
} // extern "C"