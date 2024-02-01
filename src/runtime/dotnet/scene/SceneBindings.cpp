#include <runtime/dotnet/ManagedHandle.hpp>
#include <runtime/dotnet/scene/ManagedNode.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

extern "C" {
    ManagedHandle Scene_Create()
    {
        return CreateManagedHandleFromHandle(CreateObject<Scene>());
    }

    World *Scene_GetWorld(ManagedHandle scene_handle)
    {
        return CreateHandleFromManagedHandle<Scene>(scene_handle)->GetWorld();
    }

    ManagedNode Scene_GetRoot(ManagedHandle scene_handle)
    {
        Handle<Scene> scene = CreateHandleFromManagedHandle<Scene>(scene_handle);

        if (!scene) {
            return { nullptr };
        }

        return CreateManagedNodeFromNodeProxy(scene->GetRoot());
    }

    EntityManager *Scene_GetEntityManager(ManagedHandle scene_handle)
    {
        return CreateHandleFromManagedHandle<Scene>(scene_handle)->GetEntityManager().Get();
    }
}