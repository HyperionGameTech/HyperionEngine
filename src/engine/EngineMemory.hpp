/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {

namespace memory {
class Pool;
} // namespace memory

using memory::Pool;

namespace threading {
class ThreadId;
} // namespace threading

using threading::ThreadId;

HYP_ENUM()
enum EnginePoolName : uint32
{
    EPN_NONE = 0,
    EPN_CORE,
    EPN_RENDER,
    EPN_SCENE,

    EPN_MAX
};

HYP_API extern Pool* g_enginePools[EPN_MAX];

HYP_API extern void EngineMemory_Initialize();
HYP_API extern void EngineMemory_Shutdown();

HYP_API extern const ThreadId& EngineMemory_GetPoolThreadId(EnginePoolName poolName);
HYP_API extern Pool* EngineMemory_GetPool(EnginePoolName poolName);

} // namespace hyperion
