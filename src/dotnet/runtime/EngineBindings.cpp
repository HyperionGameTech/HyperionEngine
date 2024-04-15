/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

extern "C" {
HYP_EXPORT Engine *Engine_GetInstance()
{
    return g_engine;
}

HYP_EXPORT World *Engine_GetWorld(Engine *engine)
{
    const Handle<World> &world = engine->GetWorld();
    AssertThrow(world.IsValid());

    return world.Get();
}
} // extern "C"