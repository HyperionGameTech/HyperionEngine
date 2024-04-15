/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/scene/ManagedNode.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/lib/TypeID.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT uint32 Scene_GetTypeID()
{
    return TypeID::ForType<Scene>().Value();
}

HYP_EXPORT void Scene_Create(ManagedHandle *handle)
{
    *handle = CreateManagedHandleFromHandle(CreateObject<Scene>());
}

HYP_EXPORT World *Scene_GetWorld(ManagedHandle scene_handle)
{
    return CreateHandleFromManagedHandle<Scene>(scene_handle)->GetWorld();
}

HYP_EXPORT void Scene_GetRoot(ManagedHandle scene_handle, ManagedNode *root)
{
    Handle<Scene> scene = CreateHandleFromManagedHandle<Scene>(scene_handle);
    AssertThrow(scene.IsValid());

    *root = CreateManagedNodeFromNodeProxy(scene->GetRoot());
}

HYP_EXPORT EntityManager *Scene_GetEntityManager(ManagedHandle scene_handle)
{
    return CreateHandleFromManagedHandle<Scene>(scene_handle)->GetEntityManager().Get();
}
} // extern "C"