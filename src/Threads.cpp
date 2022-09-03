#include "Threads.hpp"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadId> Threads::thread_ids {
    decltype(thread_ids)::Pair { THREAD_MAIN,    ThreadId { static_cast<UInt>(THREAD_MAIN),    "MainThread" } },
    // decltype(thread_ids)::Pair { THREAD_RENDER   ThreadId { static_cast<UInt>(THREAD_RENDER),  "RenderThread" } },
    decltype(thread_ids)::Pair { THREAD_GAME,    ThreadId { static_cast<UInt>(THREAD_GAME),    "GameThread" } },
    decltype(thread_ids)::Pair { THREAD_TERRAIN, ThreadId { static_cast<UInt>(THREAD_TERRAIN), "TerrainGenerationThread" } }
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
        "Expected current thread to be in mask %u, but got %u (%s)",
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

ThreadId Threads::GetCurrentThreadID()
{
    return current_thread_id;
}

} // namespace hyperion::v2