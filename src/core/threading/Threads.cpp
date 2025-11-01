/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace hyperion {

HYP_API const StaticThreadId g_mainThread = StaticThreadId(NAME("Main"));
HYP_API const StaticThreadId g_renderThread = g_mainThread;
HYP_API const StaticThreadId g_gameThread = StaticThreadId(NAME("Game"));

namespace threading {

static const ThreadId& ThreadSet_KeyBy(ThreadBase* thread)
{
    return thread->Id();
}

class ThreadMap
{
public:
    using ThreadSetType = HashSet<ThreadBase*, &ThreadSet_KeyBy>;

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

static ThreadMap g_staticThreadMap = {};
static ThreadMap g_dynamicThreadMap = {};

thread_local ThreadBase* g_currentThread = nullptr;

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadId g_currentThreadId = ThreadId::Invalid();
#else
static const ThreadId g_currentThreadId = ThreadId::Invalid();
#endif

void Threads::SetCurrentThreadId(const ThreadId& id)
{
#ifdef HYP_ENABLE_THREAD_ID
    g_currentThreadId = id;
#endif

#ifdef HYP_WINDOWS
    HRESULT setThreadResult = SetThreadDescription(
        GetCurrentThread(),
        &HYP_UTF8_TOWIDE(id.GetName().LookupString())[0]);

    if (FAILED(setThreadResult))
    {
        HYP_LOG(Threading, Error, "Failed to set Win32 thread name for thread {}", id.GetName());
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(id.GetName().LookupString());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), id.GetName().LookupString());
#endif
}

void Threads::RegisterThread(const ThreadId& id, ThreadBase* thread)
{
    AssertDebug(id.IsValid());
    AssertDebug(thread != nullptr);

    bool success = false;

    if (id.IsDynamic())
    {
        success = g_dynamicThreadMap.Add(thread);
    }
    else
    {
        success = g_staticThreadMap.Add(thread);
    }

    AssertDebug(success, "Thread {} ({}) could not be registered",
        id.GetValue(), *id.GetName());
}

void Threads::UnregisterThread(const ThreadId& id)
{
    if (!id.IsValid())
    {
        return;
    }

    if (id.IsDynamic())
    {
        g_dynamicThreadMap.Remove(id);
    }
    else
    {
        g_staticThreadMap.Remove(id);
    }
}

bool Threads::IsThreadRegistered(const ThreadId& id)
{
    if (!id.IsValid())
    {
        return false;
    }

    if (id.IsDynamic())
    {
        return g_dynamicThreadMap.Get(id) != nullptr;
    }
    else
    {
        return g_staticThreadMap.Get(id) != nullptr;
    }
}

ThreadBase* Threads::GetThread(const ThreadId& threadId)
{
    if (!threadId.IsValid())
    {
        return nullptr;
    }

    if (threadId.IsDynamic())
    {
        return g_dynamicThreadMap.Get(threadId);
    }
    else
    {
        return g_staticThreadMap.Get(threadId);
    }
}

ThreadBase* Threads::CurrentThreadObject()
{
    return g_currentThread;
}

void Threads::SetCurrentThreadObject(ThreadBase* thread)
{
    AssertDebug(thread != nullptr);

    AssertDebug(IsThreadRegistered(thread->Id()), "Thread %u (%s) is not registered",
        thread->Id().GetValue(), *thread->Id().GetName());

    g_currentThread = thread;

    SetCurrentThreadId(thread->Id());
    SetCurrentThreadPriority(thread->GetPriority());
}

void Threads::AssertOnThread(ThreadMask mask, const char* message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    thread_local const ThreadId& currentThreadId = CurrentThreadId();

    AssertDebug(
        mask & currentThreadId.GetMask(),
        "Expected current thread to be in mask {}, but got {} ({}). Message: {}",
        mask,
        currentThreadId.GetMask(),
        currentThreadId.GetName().LookupString(),
        message ? message : "(no message)");

#else
    HYP_LOG(Threading, Error, "AssertOnThread() called but thread IDs are currently disabled");
#endif
#endif
}

void Threads::AssertOnThread(const ThreadId& threadId, const char* message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    thread_local const ThreadId& currentThreadId = CurrentThreadId();

    AssertDebug(
        threadId == currentThreadId,
        "Expected current thread to be {}, but got {}. Message: {}",
        threadId.GetName().LookupString(),
        currentThreadId.GetName().LookupString(),
        message ? message : "(no message)");
#else
    HYP_LOG(Threading, Error, "AssertOnThread() called but thread IDs are currently disabled!");
#endif
#endif
}

bool Threads::IsThreadInMask(const ThreadId& threadId, ThreadMask mask)
{
    return mask & threadId.GetMask();
}

bool Threads::IsOnThread(ThreadMask mask)
{
#ifdef HYP_ENABLE_THREAD_ID
    thread_local const ThreadId& currentThreadId = CurrentThreadId();

    if (mask & currentThreadId.GetMask())
    {
        return true;
    }

#else
    HYP_LOG(Threading, Error, "IsOnThread() called but thread IDs are currently disabled!");
#endif

    return false;
}

bool Threads::IsOnThread(const ThreadId& threadId)
{
#ifdef HYP_ENABLE_THREAD_ID
    thread_local const ThreadId& currentThreadId = CurrentThreadId();

    if (threadId == currentThreadId)
    {
        return true;
    }

#else
    HYP_LOG(Threading, Error, "IsOnThread() called but thread IDs are currently disabled!");
#endif

    return false;
}

const ThreadId& Threads::CurrentThreadId()
{
    // For non-thread object threads (e.g .NET finalizer threads),
    // read the thread name from the OS and allocate a new thread Id.
    // SetCurrentThreadId() should be called before CurrentThreadId() for any threads that should not use the OS-created name.
    if (!g_currentThreadId.IsValid())
    {
#ifdef HYP_WINDOWS
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
            g_currentThreadId = ThreadId(CreateNameFromDynamicString(&threadNameMb[0]), /* forceUnique */ true);
        }
        else
        {
            g_currentThreadId = ThreadId(NAME("Unknown"), /* forceUnique */ true);
        }
#elif HYP_UNIX
        char threadName[256];
        pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

        g_currentThreadId = ThreadId(CreateNameFromDynamicString(threadName), /* forceUnique */ true);
#endif
    }

    return g_currentThreadId;
}

void Threads::SetCurrentThreadPriority(ThreadPriorityValue priority)
{
#ifdef HYP_WINDOWS
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
#elif defined(HYP_LINUX) || defined(HYP_APPLE)
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

uint32 Threads::NumCores() /// TODO: Refactor thread affinity setting per-thread
{
#ifdef HYP_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(HYP_UNIX)
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

void Threads::Sleep(uint32 milliseconds)
{
#ifdef HYP_WINDOWS
    ::Sleep(milliseconds);
#elif defined(HYP_UNIX)
    usleep(milliseconds * 1000);
#endif
}

} // namespace threading
} // namespace hyperion