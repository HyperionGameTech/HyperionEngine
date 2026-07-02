/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Logging/Logger.hpp>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

#if HYP_WINDOWS
#include <winnt.h>
#elif HYP_UNIX
#include <unistd.h>
#include <sched.h>
#endif

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Threading);

const StaticThreadId g_mainThread = StaticThreadId(NAME("MainThread"));

// set in Hyp_Initialize()
StaticThreadId g_simThread;
StaticThreadId g_renderThread;
StaticThreadId g_visThread;

namespace threading {

static const ThreadId& ThreadSet_KeyBy(ThreadBase* thread)
{
    return thread->Id();
}

class ThreadMap
{
public:
    using ThreadSetType = THashTable<ThreadBase*, &ThreadSet_KeyBy>;

    ThreadMap() = default;

    ThreadMap(const ThreadSetType& threads)
        : m_threads(threads)
    {
    }

    ThreadMap(const ThreadMap& other) = delete;
    ThreadMap& operator=(const ThreadMap& other) = delete;

    ThreadBase* Get(const ThreadId& threadId)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(threadId);

        if (it == m_threads.End())
        {
            return nullptr;
        }

        return *it;
    }

    /*! \brief Add a thread to the map. Returns false if the thread is already in the map. Returns true on success. */
    bool Add(ThreadBase* thread)
    {
        HYP_CORE_ASSERT(thread != nullptr);

        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(thread->Id());

        if (it != m_threads.End())
        {
            return false;
        }

        m_threads.Set(thread);

        return true;
    }

    /*! \brief Remove a thread from the map. Returns false if the thread is not in the map. Returns true on success. */
    bool Remove(const ThreadId& threadId)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(threadId);

        if (it == m_threads.End())
        {
            return false;
        }

        m_threads.Erase(it);

        return true;
    }

private:
    ThreadSetType m_threads;
    Mutex m_mutex;
};

static ThreadMap s_staticThreadMap = {};
static ThreadMap s_dynamicThreadMap = {};

thread_local ThreadBase* t_currentThread = nullptr;
thread_local ThreadId t_currentThreadId = ThreadId::Invalid();
thread_local uint32 t_currentThreadIndex = 0;

void SetCurrentThreadId(const ThreadId& id)
{
    t_currentThreadId = id;

#if HYP_WINDOWS
    HRESULT setThreadResult = SetThreadDescription(
        GetCurrentThread(),
        WideString(id.GetName().LookupString()).Data());

    if (FAILED(setThreadResult))
    {
        HYP_LOG(Threading, Error, "Failed to set Win32 thread name for thread {}", id.GetName());
    }
#elif HYP_MACOS || HYP_IOS
    pthread_setname_np(id.GetName().LookupString());
#elif HYP_UNIX
    pthread_setname_np(pthread_self(), id.GetName().LookupString());
#endif
}

void RegisterThread(const ThreadId& id, ThreadBase* thread)
{
    AssertDebug(id.IsValid());
    AssertDebug(thread != nullptr);

    bool success = false;

    if (id.IsDynamic())
    {
        success = s_dynamicThreadMap.Add(thread);
    }
    else
    {
        success = s_staticThreadMap.Add(thread);
    }

    AssertDebug(success, "Thread {} ({}) could not be registered",
                id.GetValue(), *id.GetName());
}

void UnregisterThread(const ThreadId& id)
{
    if (!id.IsValid())
    {
        return;
    }

    if (id.IsDynamic())
    {
        s_dynamicThreadMap.Remove(id);
    }
    else
    {
        s_staticThreadMap.Remove(id);
    }
}

bool IsThreadRegistered(const ThreadId& id)
{
    if (!id.IsValid())
    {
        return false;
    }

    if (id.IsDynamic())
    {
        return s_dynamicThreadMap.Get(id) != nullptr;
    }
    else
    {
        return s_staticThreadMap.Get(id) != nullptr;
    }
}

ThreadBase* GetThreadById(const ThreadId& threadId)
{
    if (!threadId.IsValid())
    {
        return nullptr;
    }

    if (threadId.IsDynamic())
    {
        return s_dynamicThreadMap.Get(threadId);
    }
    else
    {
        return s_staticThreadMap.Get(threadId);
    }
}

ThreadBase* CurrentThreadObject()
{
    return t_currentThread;
}

void SetCurrentThreadObject(ThreadBase* thread)
{
    AssertDebug(thread != nullptr);

    AssertDebug(IsThreadRegistered(thread->Id()), "Thread %u (%s) is not registered",
                thread->Id().GetValue(), *thread->Id().GetName());

    t_currentThread = thread;

    SetCurrentThreadId(thread->Id());
    SetCurrentThreadPriority(thread->GetPriority());
}

uint32 GetCurrentThreadIndex()
{
    return t_currentThreadIndex;
}

void SetCurrentThreadIndex(uint32 threadIndex)
{
    t_currentThreadIndex = threadIndex;
}

#if HYP_DEBUG_MODE

void AssertOnThread(ThreadMask mask, const char* message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
    const ThreadId& currentThreadId = CurrentThreadId();

    AssertDebug(
        mask & currentThreadId.GetMask(),
        "Expected current thread to be in mask {}, but got {} ({}). Message: {}",
        mask,
        currentThreadId.GetMask(),
        currentThreadId.GetName().LookupString(),
        message ? message : "(no message)");
#endif
}

void AssertOnThread(const ThreadId& threadId, const char* message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
    const ThreadId& currentThreadId = CurrentThreadId();

    AssertDebug(
        threadId == currentThreadId,
        "Expected current thread to be {} ({}), but got {} ({}). Message: {}",
        threadId.GetName().LookupString(),
        threadId.GetValue(),
        currentThreadId.GetName().LookupString(),
        currentThreadId.GetValue(),
        message ? message : "(no message)");
#endif
}

#endif // HYP_DEBUG_MODE

bool IsThreadInMask(const ThreadId& threadId, ThreadMask mask)
{
    return mask & threadId.GetMask();
}

bool IsOnThread(ThreadMask mask)
{
    const ThreadId& currentThreadId = CurrentThreadId();

    if (mask & currentThreadId.GetMask())
    {
        return true;
    }

    return false;
}

bool IsOnThread(const ThreadId& threadId)
{
    const ThreadId& currentThreadId = CurrentThreadId();

    if (threadId == currentThreadId)
    {
        return true;
    }

    return false;
}

const ThreadId& CurrentThreadId()
{
    // For non-thread object threads (e.g .NET finalizer threads),
    // read the thread name from the OS and allocate a new thread Id.
    // SetCurrentThreadId() should be called before CurrentThreadId() for any threads that should not use the OS-created name.
    if (!t_currentThreadId.IsValid())
    {
#if HYP_WINDOWS
        PWCHAR threadName[256];
        HRESULT result = GetThreadDescription(GetCurrentThread(), &threadName[0]);

        char threadNameMb[256];
        WideCharToMultiByte(
            CP_ACP,
            0,
            threadName[0],
            -1,
            threadNameMb,
            sizeof(threadNameMb),
            nullptr,
            nullptr);

        if (SUCCEEDED(result))
        {
            t_currentThreadId = ThreadId(CreateNameFromDynamicString(&threadNameMb[0]), /* forceUnique */ true);
        }
        else
        {
            t_currentThreadId = ThreadId(NAME("Unknown"), /* forceUnique */ true);
        }
#elif HYP_UNIX
        char threadName[256];
        pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

        t_currentThreadId = ThreadId(CreateNameFromDynamicString(threadName), /* forceUnique */ true);
#endif
    }

    return t_currentThreadId;
}

void SetCurrentThreadPriority(ThreadPriorityValue priority)
{
#if HYP_WINDOWS
    int winPriority = THREAD_PRIORITY_NORMAL;

    switch (priority)
    {
    case ThreadPriorityValue::LOWEST:
        winPriority = THREAD_PRIORITY_LOWEST;
        break;

    case ThreadPriorityValue::LOW:
        winPriority = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case ThreadPriorityValue::NORMAL:
        winPriority = THREAD_PRIORITY_NORMAL;
        break;

    case ThreadPriorityValue::HIGH:
        winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case ThreadPriorityValue::HIGHEST:
        winPriority = THREAD_PRIORITY_HIGHEST;
        break;
    }

    SetThreadPriority(GetCurrentThread(), winPriority);
#elif HYP_UNIX
    int policy = SCHED_OTHER;
    struct sched_param param;

    switch (priority)
    {
    case ThreadPriorityValue::LOWEST:
        param.sched_priority = sched_get_priority_min(policy);
        break;

    case ThreadPriorityValue::LOW:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 4;
        break;

    case ThreadPriorityValue::NORMAL:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 2;
        break;

    case ThreadPriorityValue::HIGH:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) * 3 / 4;
        break;

    case ThreadPriorityValue::HIGHEST:
        param.sched_priority = sched_get_priority_max(policy);
        break;
    }

    pthread_setschedparam(pthread_self(), policy, &param);
#endif
}

uint32 NumCores() /// TODO: Refactor thread affinity setting per-thread
{
#if HYP_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif HYP_UNIX
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

void ThreadSleep(uint32 milliseconds)
{
#if HYP_WINDOWS
    ::Sleep(milliseconds);
#elif HYP_UNIX
    usleep(milliseconds * 1000);
#endif
}

void ThreadYield()
{
#if HYP_WINDOWS
    YieldProcessor();
#elif HYP_UNIX
    sched_yield();
#endif
}

} // namespace threading
} // namespace Hyperion
