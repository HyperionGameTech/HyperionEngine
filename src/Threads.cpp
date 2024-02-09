#include "Threads.hpp"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadID> Threads::thread_ids = {
    { THREAD_MAIN,      ThreadID { uint(THREAD_MAIN),       HYP_NAME_UNSAFE(MainThread) } },
    { THREAD_GAME,      ThreadID { uint(THREAD_GAME),       HYP_NAME_UNSAFE(GameThread) } },
    { THREAD_TERRAIN,   ThreadID { uint(THREAD_TERRAIN),    HYP_NAME_UNSAFE(TerrainGenerationThread) } },
    { THREAD_TASK_0,    ThreadID { uint(THREAD_TASK_0),     HYP_NAME_UNSAFE(TaskThread0) } },
    { THREAD_TASK_1,    ThreadID { uint(THREAD_TASK_1),     HYP_NAME_UNSAFE(TaskThread1) } },
    { THREAD_TASK_2,    ThreadID { uint(THREAD_TASK_2),     HYP_NAME_UNSAFE(TaskThread2) } },
    { THREAD_TASK_3,    ThreadID { uint(THREAD_TASK_3),     HYP_NAME_UNSAFE(TaskThread3) } },
    { THREAD_TASK_4,    ThreadID { uint(THREAD_TASK_4),     HYP_NAME_UNSAFE(TaskThread4) } },
    { THREAD_TASK_5,    ThreadID { uint(THREAD_TASK_5),     HYP_NAME_UNSAFE(TaskThread5) } },
    { THREAD_TASK_6,    ThreadID { uint(THREAD_TASK_6),     HYP_NAME_UNSAFE(TaskThread6) } },
    { THREAD_TASK_7,    ThreadID { uint(THREAD_TASK_7),     HYP_NAME_UNSAFE(TaskThread7) } },
};

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID current_thread_id = ThreadID { uint(THREAD_MAIN), HYP_NAME_UNSAFE(MainThread) };
#else
static const ThreadID current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif


void Threads::SetThreadID(const ThreadID &id)
{
#ifdef HYP_ENABLE_THREAD_ID
    current_thread_id = id;
#endif
}

void Threads::AssertOnThread(ThreadMask mask, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        (mask & current.value),
        "Expected current thread to be in mask %u, but got %u (%s). Message: %s",
        mask,
        current.value,
        current.name.LookupString(),
        message ? message : "(no message)"
    );

#else
    DebugLog(
        LogType::Error,
        "AssertOnThread() called but thread IDs are currently disabled!\n"
    );
#endif
#endif
}

void Threads::AssertOnThread(const ThreadID &thread_id, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        thread_id == current,
        "Expected current thread to be %u (%s), but got %u (%s). Message: %s",
        thread_id.value,
        thread_id.name.LookupString(),
        current.value,
        current.name.LookupString(),
        message ? message : "(no message)"
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

ThreadType Threads::GetThreadType()
{
    const uint thread_id = Threads::CurrentThreadID().value;

    return thread_id == THREAD_GAME ? THREAD_TYPE_GAME
        : (thread_id == THREAD_RENDER ? THREAD_TYPE_RENDER : THREAD_TYPE_INVALID);
}

SizeType Threads::NumCores()
{
    return std::thread::hardware_concurrency();
}

void Threads::Sleep(uint32 milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace hyperion::v2