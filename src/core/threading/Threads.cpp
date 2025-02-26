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

HYP_API const StaticThreadID g_main_thread = StaticThreadID(NAME("Main"));
HYP_API const StaticThreadID g_render_thread = g_main_thread;

HYP_API const StaticThreadID g_game_thread = StaticThreadID(NAME("Game"));

namespace threading {

static const ThreadID &ThreadSet_KeyBy(IThread *thread)
{
    return thread->GetID();
}

class ThreadMap
{
public:
    using ThreadSetType = HashSet<IThread *, &ThreadSet_KeyBy>;

    ThreadMap()                                     = default;

    ThreadMap(const ThreadSetType &threads)
        : m_threads(threads)
    {
    }

    ThreadMap(const ThreadMap &other)               = delete;
    ThreadMap &operator=(const ThreadMap &other)    = delete;

    IThread *Get(const ThreadID &thread_id)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(thread_id);

        if (it == m_threads.End()) {
            return nullptr;
        }

        return *it;
    }

    /*! \brief Add a thread to the map. Returns false if the thread is already in the map. Returns true on success. */
    bool Add(IThread *thread)
    {
        AssertThrow(thread != nullptr);

        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(thread->GetID());

        if (it != m_threads.End()) {
            return false;
        }

        m_threads.Set(thread);

        return true;
    }

    /*! \brief Remove a thread from the map. Returns false if the thread is not in the map. Returns true on success. */
    bool Remove(const ThreadID &thread_id)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_threads.Find(thread_id);

        if (it == m_threads.End()) {
            return false;
        }

        m_threads.Erase(it);

        return true;
    }

private:
    ThreadSetType   m_threads;
    Mutex           m_mutex;
};


static ThreadMap g_static_thread_map = { };
static ThreadMap g_dynamic_thread_map = { };

thread_local IThread *g_current_thread = nullptr;

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID g_current_thread_id = g_main_thread;
#else
static const ThreadID g_current_thread_id = g_main_thread;
#endif

static void SetCurrentThreadID(ThreadID id)
{
#ifdef HYP_ENABLE_THREAD_ID
    g_current_thread_id = id;
#endif

#ifdef HYP_WINDOWS
    HRESULT set_thread_result = SetThreadDescription(
        GetCurrentThread(),
        &HYP_UTF8_TOWIDE(id.GetName().LookupString())[0]
    );

    if (FAILED(set_thread_result)) {
        HYP_LOG(Threading, Error, "Failed to set Win32 thread name for thread {}", id.GetName());
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(id.GetName().LookupString());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), id.GetName().LookupString());
#endif
}

void Threads::RegisterThread(const ThreadID &id, IThread *thread)
{
    AssertThrow(id.IsValid());
    AssertThrow(thread != nullptr);

    bool success = false;

    if (id.IsDynamic()) {
        success = g_dynamic_thread_map.Add(thread);
    } else {
        success = g_static_thread_map.Add(thread);
    }

    AssertThrowMsg(success, "Thread %u (%s) could not be registered",
        id.GetValue(), *id.GetName());
}

void Threads::UnregisterThread(const ThreadID &id)
{
    if (!id.IsValid()) {
        return;
    }

    if (id.IsDynamic()) {
        g_dynamic_thread_map.Remove(id);
    } else {
        g_static_thread_map.Remove(id);
    }
}

bool Threads::IsThreadRegistered(const ThreadID &id)
{
    if (!id.IsValid()) {
        return false;
    }

    if (id.IsDynamic()) {
        return g_dynamic_thread_map.Get(id) != nullptr;
    } else {
        return g_static_thread_map.Get(id) != nullptr;
    }
}

IThread *Threads::GetThread(const ThreadID &thread_id)
{
    if (!thread_id.IsValid()) {
        return nullptr;
    }

    if (thread_id.IsDynamic()) {
        return g_dynamic_thread_map.Get(thread_id);
    } else {
        return g_static_thread_map.Get(thread_id);
    }
}

IThread *Threads::CurrentThreadObject()
{
    return g_current_thread;
}

void Threads::SetCurrentThreadObject(IThread *thread)
{
    AssertThrow(thread != nullptr);

    AssertThrowMsg(IsThreadRegistered(thread->GetID()), "Thread %u (%s) is not registered",
        thread->GetID().GetValue(), *thread->GetID().GetName());

    g_current_thread = thread;

    SetCurrentThreadID(thread->GetID());
    SetCurrentThreadPriority(thread->GetPriority());
}

void Threads::AssertOnThread(ThreadMask mask, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const ThreadID current_thread_id = g_current_thread_id;

    AssertThrowMsg(
        mask & current_thread_id.GetMask(),
        "Expected current thread to be in mask %u, but got %u (%s). Message: %s",
        mask,
        current_thread_id.GetMask(),
        current_thread_id.GetName().LookupString(),
        message ? message : "(no message)"
    );

#else
    HYP_LOG(Threading, Error, "AssertOnThread() called but thread IDs are currently disabled");
#endif
#endif
}

void Threads::AssertOnThread(const ThreadID &thread_id, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const ThreadID current_thread_id = g_current_thread_id;

    AssertThrowMsg(
        thread_id == current_thread_id,
        "Expected current thread to be %s, but got %s. Message: %s",
        thread_id.GetName().LookupString(),
        current_thread_id.GetName().LookupString(),
        message ? message : "(no message)"
    );
#else
    HYP_LOG(Threading, Error, "AssertOnThread() called but thread IDs are currently disabled!");
#endif
#endif
}

bool Threads::IsThreadInMask(const ThreadID &thread_id, ThreadMask mask)
{
    return mask & thread_id.GetMask();
}

bool Threads::IsOnThread(ThreadMask mask)
{
#ifdef HYP_ENABLE_THREAD_ID
    const ThreadID &current_thread_id = g_current_thread_id;

    if (mask & current_thread_id.GetMask()) {
        return true;
    }

#else
    HYP_LOG(Threading, Error, "IsOnThread() called but thread IDs are currently disabled!");
#endif

    return false;
}

bool Threads::IsOnThread(const ThreadID &thread_id)
{
#ifdef HYP_ENABLE_THREAD_ID
    if (thread_id == g_current_thread_id) {
        return true;
    }

#else
    HYP_LOG(Threading, Error, "IsOnThread() called but thread IDs are currently disabled!");
#endif

    return false;
}

const ThreadID &Threads::CurrentThreadID()
{
    return g_current_thread_id;
}

ThreadType Threads::CurrentThreadType()
{
    const ThreadID &thread_id = Threads::CurrentThreadID();

    if (thread_id == g_game_thread) {
        return THREAD_TYPE_GAME;
    }

    if (thread_id == g_render_thread) {
        return THREAD_TYPE_RENDER;
    }

    switch (thread_id.GetCategory()) {
    case THREAD_CATEGORY_TASK:
        return THREAD_TYPE_TASK;
    default:
        if (thread_id.IsDynamic()) {
            return THREAD_TYPE_DYNAMIC;
        }

        break;
    }

    return THREAD_TYPE_INVALID;
}

void Threads::SetCurrentThreadPriority(ThreadPriorityValue priority)
{
#ifdef HYP_WINDOWS
    int win_priority = THREAD_PRIORITY_NORMAL;

    switch (priority) {
    case ThreadPriorityValue::LOWEST:
        win_priority = THREAD_PRIORITY_LOWEST;
        break;

    case ThreadPriorityValue::LOW:
        win_priority = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case ThreadPriorityValue::NORMAL:
        win_priority = THREAD_PRIORITY_NORMAL;
        break;

    case ThreadPriorityValue::HIGH:
        win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case ThreadPriorityValue::HIGHEST:
        win_priority = THREAD_PRIORITY_HIGHEST;
        break;
    }

    SetThreadPriority(GetCurrentThread(), win_priority);
#elif defined(HYP_LINUX) || defined(HYP_APPLE)
    int policy = SCHED_OTHER;
    struct sched_param param;

    switch (priority) {
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

SizeType Threads::NumCores()
{
    return std::thread::hardware_concurrency();
}

void Threads::Sleep(uint32 milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace threading
} // namespace hyperion