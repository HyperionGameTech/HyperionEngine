/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Core.hpp>

#include <Engine.hpp>

namespace hyperion {

using namespace renderer;

Engine *GetEngine()
{
    return g_engine;
}

Device *GetEngineDevice()
{
    return g_engine->GetGPUDevice();
}

} // namespace hyperion