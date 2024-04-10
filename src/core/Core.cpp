#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace renderer;

HYP_API Engine *GetEngine()
{
    return g_engine;
}

HYP_API Device *GetEngineDevice()
{
    return g_engine->GetGPUInstance()->GetDevice();
}

HYP_API ObjectPool &GetObjectPool()
{
    return g_engine->GetObjectPool();
}

} // namespace hyperion::v2