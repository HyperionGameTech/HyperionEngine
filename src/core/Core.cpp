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
    return Engine::Get()->GetInstance()->GetDevice();
}

ComponentSystem &GetObjectSystem()
{
    return Engine::Get()->GetObjectSystem();
}

} // namespace hyperion::v2