/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Scheduler.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/IdGenerator.hpp>

#include <Core/Defines.hpp>

#include <Core/Math/MathUtil.hpp>

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

    // Iterate FILO
    for (size_t index = m_onExitCallbacks.Size(); index > 0; index--)
    {
        m_onExitCallbacks[index - 1]();
    }

    m_onExitCallbacks.Clear();
}

#pragma endregion ThreadBase
} // namespace threading

//#ifndef HYP_MSVC
template class CORE_API threading::Thread<threading::Scheduler>;
//#endif // HYP_MSVC

} // namespace Hyperion
