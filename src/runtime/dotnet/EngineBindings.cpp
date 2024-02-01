#include <runtime/dotnet/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

extern "C" {
    Engine *Engine_GetInstance()
    {
        return g_engine;
    }

    World *Engine_GetWorld(Engine *engine)
    {
        const Handle<World> &world = engine->GetWorld();
        AssertThrow(world.IsValid());

        return world.Get();
    }
}