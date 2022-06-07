#include "threads.h"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadId> Threads::thread_ids{
    std::make_pair(THREAD_MAIN, ThreadId{static_cast<uint>(THREAD_MAIN), "MainThread"}),
    std::make_pair(THREAD_GAME, ThreadId{static_cast<uint>(THREAD_GAME), "GameThread"})
};

#if HYP_ENABLE_THREAD_ASSERTION
thread_local ThreadId current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif

void Threads::AssertOnThread(ThreadMask mask)
{
#if HYP_ENABLE_THREAD_ASSERTION
    const auto &current = current_thread_id;

    AssertThrowMsg(
        (mask & current.value),
        "Expected current thread to be in mask %u\nBut got \"%s\" (%u)",
        mask,
        current.name.CString(),
        current.value
    );
#endif
}

bool Threads::IsOnThread(ThreadMask mask)
{
#if HYP_ENABLE_THREAD_ASSERTION
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

ThreadId Threads::GetThreadId(ThreadName thread_name)
{
    return thread_ids.At(thread_name);
}

} // namespace hyperion::v2