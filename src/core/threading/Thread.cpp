/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/ThreadLocalStorage.hpp>
#include <core/threading/Mutex.hpp>

#include <core/functional/Delegate.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/GlobalContext.hpp>
#include <core/utilities/IdGenerator.hpp>

#include <core/profiling/ProfileScope.hpp>

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

thread_local Delegate<void>* g_onThreadExit;

HYP_API void OnCurrentThreadExit()
{
    if (g_onThreadExit)
    {
        (*g_onThreadExit)();
        
        delete g_onThreadExit;
        g_onThreadExit = nullptr;
    }
}

ThreadBase::ThreadBase(const ThreadId& id, ThreadPriorityValue priority)
    : m_id(id),
      m_priority(priority),
      m_tls(nullptr)
{
    HYP_CORE_ASSERT(id.IsValid(), "ThreadId must be valid");

    RegisterThread(m_id, this);
}

ThreadBase::~ThreadBase()
{
    UnregisterThread(m_id);

    if (m_tls)
    {
        delete m_tls;
        m_tls = nullptr;
    }
}

ThreadLocalStorage& ThreadBase::GetTLS() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_id);
    
    if (!m_tls)
    {
        m_tls = new ThreadLocalStorage();
    }
    
    return *m_tls;
}

void ThreadBase::AtExit(Proc<void()>&& proc)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_id);
    
    if (!g_onThreadExit)
    {
        g_onThreadExit = new Delegate<void>();
    }
    
    g_onThreadExit->Bind(std::move(proc)).Detach();
}

#pragma endregion ThreadBase

} // namespace threading
} // namespace hyperion
