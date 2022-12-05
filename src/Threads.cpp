#include "Threads.hpp"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadID> Threads::thread_ids = {
    decltype(thread_ids)::KeyValuePair { THREAD_MAIN, ThreadID { static_cast<UInt>(THREAD_MAIN), StringView("MainThread") } },
    // decltype(thread_ids)::Pair { THREAD_RENDER   ThreadID { static_cast<UInt>(THREAD_RENDER),  "RenderThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_GAME, ThreadID { static_cast<UInt>(THREAD_GAME), StringView("GameThread") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TERRAIN, ThreadID { static_cast<UInt>(THREAD_TERRAIN), StringView("TerrainGenerationThread") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_0, ThreadID { static_cast<UInt>(THREAD_TASK_0), StringView("TaskThread0") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_1, ThreadID { static_cast<UInt>(THREAD_TASK_1), StringView("TaskThread1") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_2, ThreadID { static_cast<UInt>(THREAD_TASK_2), StringView("TaskThread2") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_3, ThreadID { static_cast<UInt>(THREAD_TASK_3), StringView("TaskThread3") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_4, ThreadID { static_cast<UInt>(THREAD_TASK_4), StringView("TaskThread4") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_5, ThreadID { static_cast<UInt>(THREAD_TASK_5), StringView("TaskThread5") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_6, ThreadID { static_cast<UInt>(THREAD_TASK_6), StringView("TaskThread6") } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_7, ThreadID { static_cast<UInt>(THREAD_TASK_7), StringView("TaskThread7") } },
};

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#else
static const ThreadID current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif


void Threads::SetThreadID(const ThreadID &id)
{
#ifdef HYP_ENABLE_THREAD_ID
    current_thread_id = id;
#endif
}

void Threads::AssertOnThread(ThreadMask mask)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        (mask & current.value),
        "Expected current thread to be in mask %u, but got %u (%s)",
        mask,
        current.value,
        current.name.Data()
    );

#else
    DebugLog(
        LogType::Error,
        "AssertOnThread() called but thread IDs are currently disabled!\n"
    );
#endif
#endif
}

void Threads::AssertOnThread(const ThreadID &thread_id)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        thread_id == current,
        "Expected current thread to be %u (%s), but got %u (%s)",
        thread_id.value,
        thread_id.name.Data(),
        current.value,
        current.name.Data()
    );
#else
    DebugLog(
        LogType::Error,
        "AssertOnThread() called but thread IDs are currently disabled!\n"
    );
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
    if (mask & current_thread_id.value) {
        return true;
    }

#else
    DebugLog(
        LogType::Error,
        "IsOnThread() called but thread IDs are currently disabled!\n"
    );
#endif

    return false;
}

bool Threads::IsOnThread(const ThreadID &thread_id)
{
#ifdef HYP_ENABLE_THREAD_ID
    if (thread_id == current_thread_id) {
        return true;
    }

#else
    DebugLog(
        LogType::Error,
        "IsOnThread() called but thread IDs are currently disabled!\n"
    );
#endif

    return false;
}

const ThreadID &Threads::GetThreadID(ThreadName thread_name)
{
    return thread_ids.At(thread_name);
}

const ThreadID &Threads::CurrentThreadID()
{
    return current_thread_id;
}

SizeType Threads::NumCores()
{
    return std::thread::hardware_concurrency();
}


} // namespace hyperion::v2