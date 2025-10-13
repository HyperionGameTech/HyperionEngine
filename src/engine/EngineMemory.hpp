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
enum EnginePoolName : int
{
    EPN_INVALID = -1,

    EPN_CORE = 0, // global shared pool
    EPN_RENDER,
    EPN_SCENE,

    EPN_MAX
};

HYP_API extern void EngineMemory_Initialize();
HYP_API extern void EngineMemory_Shutdown();

HYP_API extern const ThreadId& EngineMemory_GetPoolThreadId(EnginePoolName poolName);
HYP_API extern Pool* EngineMemory_GetPool(EnginePoolName poolName);
HYP_API extern EnginePoolName EngineMemory_GetPoolName(const char* str);

} // namespace hyperion
