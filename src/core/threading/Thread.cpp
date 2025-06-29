/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/ThreadLocalStorage.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/IdGenerator.hpp>

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

HYP_API void RegisterThread(const ThreadId& id, ThreadBase* thread)
{
    Threads::RegisterThread(id, thread);
}

HYP_API void UnregisterThread(const ThreadId& id)
{
    Threads::UnregisterThread(id);
}

HYP_API void SetCurrentThreadObject(ThreadBase* thread)
{
    Threads::SetCurrentThreadObject(thread);
}

HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority)
{
    Threads::SetCurrentThreadPriority(priority);
}

#pragma region ThreadBase

ThreadBase::ThreadBase(const ThreadId& id, ThreadPriorityValue priority)
    : m_id(id),
      m_priority(priority),
      m_tls(new ThreadLocalStorage())
{
    AssertThrowMsg(id.IsValid(), "ThreadId must be valid");

    RegisterThread(m_id, this);
}

ThreadBase::~ThreadBase()
{
    UnregisterThread(m_id);

    delete m_tls;
}

#pragma endregion ThreadBase

} // namespace threading
} // namespace hyperion