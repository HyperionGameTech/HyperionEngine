#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace renderer;

Engine *GetEngine()
{
    return Engine::Get();
}

Device *GetEngineDevice()
{
    return Engine::Get()->GetGPUInstance()->GetDevice();
}

ObjectPool &GetObjectPool()
{
    return Engine::Get()->GetObjectPool();
}

} // namespace hyperion::v2