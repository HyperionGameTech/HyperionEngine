#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace renderer;

Engine *GetEngine()
{
    return Engine::Get();
}

Device *GetEngineDevice(Engine *engine)
{
    return Engine::Get()->GetInstance()->GetDevice();
}

} // namespace hyperion::v2