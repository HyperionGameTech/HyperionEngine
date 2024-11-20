/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/Mutex.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

namespace threading {

static constexpr uint32 g_static_thread_index_max = MathUtil::FastLog2_Pow2(((~(THREAD_STATIC >> 1)) & THREAD_STATIC));

struct StaticThreadMap
{
    StaticThreadMap(const FlatMap<ThreadName, Pair<ThreadID, IThread *>> &map)
    {
        for (const auto &it : map) {
            const uint32 thread_index = MathUtil::FastLog2_Pow2(it.first);
            AssertThrow(thread_index < threads.Size());

            threads[thread_index] = it.second;
        }
    }

    ThreadID GetThreadID(ThreadName thread_name)
    {
        const uint32 thread_index = MathUtil::FastLog2_Pow2(thread_name);
        AssertThrow(thread_index < threads.Size());

        return threads[thread_index].first;
    }

    IThread *Get(ThreadID thread_id)
    {
        AssertThrow(!thread_id.IsDynamic());

        return threads[MathUtil::FastLog2_Pow2(thread_id.value)].second;
    }

    void Add(IThread *thread)
    {
        AssertThrow(thread != nullptr);
        AssertThrow(!thread->GetID().IsDynamic());

        threads[MathUtil::FastLog2_Pow2(thread->GetID())].second = thread;
    }

    bool Remove(ThreadID thread_id)
    {
        AssertThrow(!thread_id.IsDynamic());

        threads[MathUtil::FastLog2_Pow2(thread_id.value)].second = nullptr;

        return true;
    }

    FixedArray<Pair<ThreadID, IThread *>, g_static_thread_index_max>    threads;
};

struct DynamicThreadMap
{
    IThread *Get(ThreadID thread_id)
    {
        AssertThrow(thread_id.IsDynamic());

        Mutex::Guard guard(mutex);

        return threads[thread_id];
    }

    void Add(IThread *thread)
    {
        AssertThrow(thread != nullptr);
        AssertThrow(thread->GetID().IsDynamic());

        Mutex::Guard guard(mutex);

        threads[thread->GetID()] = thread;
    }

    bool Remove(ThreadID thread_id)
    {
        AssertThrow(thread_id.IsDynamic());

        Mutex::Guard guard(mutex);

        return threads.Erase(thread_id);
    }

    FlatMap<ThreadID, IThread *>    threads;
    Mutex                           mutex;
};

static StaticThreadMap g_static_thread_map = StaticThreadMap({
    { THREAD_MAIN,      Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_MAIN),         HYP_NAME_UNSAFE(MainThread) }, nullptr } },
    { THREAD_GAME,      Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_GAME),         HYP_NAME_UNSAFE(GameThread) }, nullptr } },
    { THREAD_RESERVED0, Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_RESERVED0),    HYP_NAME_UNSAFE(ReservedThread0) }, nullptr } },
    { THREAD_TASK_0,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_0),       HYP_NAME_UNSAFE(TaskThread0) }, nullptr } },
    { THREAD_TASK_1,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_1),       HYP_NAME_UNSAFE(TaskThread1) }, nullptr } },
    { THREAD_TASK_2,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_2),       HYP_NAME_UNSAFE(TaskThread2) }, nullptr } },
    { THREAD_TASK_3,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_3),       HYP_NAME_UNSAFE(TaskThread3) }, nullptr } },
    { THREAD_TASK_4,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_4),       HYP_NAME_UNSAFE(TaskThread4) }, nullptr } },
    { THREAD_TASK_5,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_5),       HYP_NAME_UNSAFE(TaskThread5) }, nullptr } },
    { THREAD_TASK_6,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_6),       HYP_NAME_UNSAFE(TaskThread6) }, nullptr } },
    { THREAD_TASK_7,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_7),       HYP_NAME_UNSAFE(TaskThread7) }, nullptr } },
    { THREAD_TASK_8,    Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_TASK_8),       HYP_NAME_UNSAFE(TaskThread8) }, nullptr } },
    { THREAD_RESERVED1, Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_RESERVED1),    HYP_NAME_UNSAFE(ReservedThread1) }, nullptr } },
    { THREAD_RESERVED2, Pair<ThreadID, IThread *> { ThreadID { uint32(THREAD_RESERVED2),    HYP_NAME_UNSAFE(ReservedThread2) }, nullptr } }
});

static DynamicThreadMap g_dynamic_thread_map = { };

thread_local IThread *g_current_thread = nullptr;

// @TODO heirarchical thread IDs so task thread ids can be derived from the main thread id
#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID g_current_thread_id = ThreadID { uint(THREAD_MAIN), HYP_NAME_UNSAFE(MainThread) };
#else
static const ThreadID g_current_thread_id = ThreadID { uint(THREAD_MAIN), HYP_NAME_UNSAFE(MainThread) };
#endif

static void SetCurrentThreadID(ThreadID id)
{
#ifdef HYP_ENABLE_THREAD_ID
    g_current_thread_id = id;
#endif

#ifdef HYP_WINDOWS
    HRESULT set_thread_result = SetThreadDescription(
        GetCurrentThread(),
        &HYP_UTF8_TOWIDE(id.name.LookupString())[0]
    );

    if (FAILED(set_thread_result)) {
        HYP_LOG(Threading, LogLevel::ERR, "Failed to set Win32 thread name for thread {}", id.name);
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(id.name.LookupString());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), id.name.LookupString());
#endif
}

IThread *Threads::GetThread(ThreadID thread_id)
{
    if (thread_id.IsDynamic()) {
        return g_dynamic_thread_map.Get(thread_id);
    } else {
        return g_static_thread_map.Get(thread_id);
    }

    // if (!(THREAD_TASK & thread_id.GetMask())) {
    //     return nullptr;
    // }

    // return TaskSystem::GetInstance().GetTaskThread(thread_id);
}

IThread *Threads::CurrentThreadObject()
{
    return g_current_thread;
}

void Threads::SetCurrentThreadObject(IThread *thread)
{
    AssertThrow(thread != nullptr);

    g_current_thread = thread;

    SetCurrentThreadID(thread->GetID());
    SetCurrentThreadPriority(thread->GetPriority());

    if (thread->GetID().IsDynamic()) {
        g_dynamic_thread_map.Add(thread);
    } else {
        g_static_thread_map.Add(thread);
    }
}

void Threads::AssertOnThread(ThreadMask mask, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const ThreadID current_thread_id = g_current_thread_id;

    AssertThrowMsg(
        (mask & current_thread_id.value),
        "Expected current thread to be in mask %u, but got %u (%s). Message: %s",
        mask,
        current_thread_id.value,
        current_thread_id.name.LookupString(),
        message ? message : "(no message)"
    );

#else
    HYP_LOG(Threading, LogLevel::ERR, "AssertOnThread() called but thread IDs are currently disabled");
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
        "Expected current thread to be %u (%s), but got %u (%s). Message: %s",
        thread_id.value,
        thread_id.name.LookupString(),
        current_thread_id.value,
        current_thread_id.name.LookupString(),
        message ? message : "(no message)"
    );
#else
    HYP_LOG(Threading, LogLevel::ERR, "AssertOnThread() called but thread IDs are currently disabled!");
#endif
#endif
}

bool Threads::IsThreadInMask(const ThreadID &thread_id, ThreadMask mask)
{
    if (mask & thread_id.value) {
        return true;
    }

    return false;
}

bool Threads::IsOnThread(ThreadMask mask)
{
#ifdef HYP_ENABLE_THREAD_ID
    if (mask & g_current_thread_id.value) {
        return true;
    }

#else
    HYP_LOG(Threading, LogLevel::ERR, "IsOnThread() called but thread IDs are currently disabled!");
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
    HYP_LOG(Threading, LogLevel::ERR, "IsOnThread() called but thread IDs are currently disabled!");
#endif

    return false;
}

ThreadID Threads::GetStaticThreadID(ThreadName thread_name)
{
    return g_static_thread_map.GetThreadID(thread_name);
}

ThreadID Threads::CurrentThreadID()
{
    return g_current_thread_id;
}

ThreadType Threads::CurrentThreadType()
{
    const uint thread_id = Threads::CurrentThreadID().value;

    return thread_id == THREAD_GAME ? THREAD_TYPE_GAME
        : (thread_id == THREAD_RENDER ? THREAD_TYPE_RENDER : THREAD_TYPE_INVALID);
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