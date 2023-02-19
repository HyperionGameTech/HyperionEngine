#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace renderer;

Engine *GetEngine()
{
    return g_engine;
}

Device *GetEngineDevice()
{
    return g_engine->GetGPUInstance()->GetDevice();
}

ObjectPool &GetObjectPool()
{
    return g_engine->GetObjectPool();
}

} // namespace hyperion::v2