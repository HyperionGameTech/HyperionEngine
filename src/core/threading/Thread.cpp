/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/IDGenerator.hpp>

#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>

#include <util/UTF8.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(HYP_UNIX)
#include <pthread.h>
#endif

namespace hyperion {
namespace threading {

HYP_API void SetCurrentThreadObject(IThread *thread)
{
    Threads::SetCurrentThreadObject(thread);
}

HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority)
{
    Threads::SetCurrentThreadPriority(priority);
}

} // namespace threading
} // namespace hyperion