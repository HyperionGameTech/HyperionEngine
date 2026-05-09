/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/threading/Thread.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/ThreadLocalStorage.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/containers/Map.hpp>

#include <Core/utilities/GlobalContext.hpp>
#include <Core/utilities/IdGenerator.hpp>

#include <Core/Defines.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/Unicode.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(HYP_UNIX)
#include <pthread.h>
#endif

namespace Hyperion {
namespace threading {

#pragma region ThreadBase

ThreadBase::ThreadBase(const ThreadId& id, ThreadPriorityValue priority)
    : m_id(id),
      m_priority(priority),
      m_tls(nullptr)
{
    AssertDebug(id.IsValid(), "ThreadId must be valid");

    if (GetThreadById(id) == nullptr)
    {
        RegisterThread(m_id, this);
    }
}

ThreadBase::~ThreadBase()
{
    if (GetThreadById(m_id) == this)
    {
        UnregisterThread(m_id);
    }

    if (m_tls)
    {
        delete m_tls;
        m_tls = nullptr;
    }
}

ThreadLocalStorage& ThreadBase::GetTLS() const
{
    AssertOnThread(m_id);

    if (HYP_UNLIKELY(!m_tls))
    {
        m_tls = new ThreadLocalStorage();
    }

    return *m_tls;
}

void ThreadBase::AddOnExitCallback(void (*callback)(void))
{
    if (!callback)
        return;

    Mutex::Guard guard(m_onExitMutex);
    m_onExitCallbacks.PushBack(callback);
}

void ThreadBase::OnExit()
{
    Mutex::Guard guard(m_onExitMutex);

    for (auto& cb : m_onExitCallbacks)
    {
        cb();
    }

    m_onExitCallbacks.Clear();
}

#pragma endregion ThreadBase

} // namespace threading
} // namespace Hyperion
