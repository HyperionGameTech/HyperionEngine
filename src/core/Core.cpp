#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace renderer;

GetEngine()
{
    return Engine::Get();
}

Device *GetEngineDevice()
{
    return Engine::Get()->GetInstance()->GetDevice();
}

} // namespace hyperion::v2