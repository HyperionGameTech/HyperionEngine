#include <core/Thread.hpp>
#include <Threads.hpp>
#include <util/Defines.hpp>
#include <util/UTF8.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(HYP_UNIX)
#include <pthread.h>
#endif

namespace hyperion::v2 {

const ThreadID ThreadID::invalid = ThreadID { ~0u, HYP_NAME(InvalidThreadID) };

ThreadID ThreadID::Current()
{
    return Threads::CurrentThreadID();
}

ThreadID ThreadID::CreateDynamicThreadID(Name name)
{
    struct ThreadIDGenerator
    {
        AtomicVar<uint32> counter { 0 };

        uint32 Next()
        {
            return counter.Increment(1, MemoryOrder::SEQUENTIAL) + 1;
        }
    };

    static ThreadIDGenerator generator;

    return { generator.Next() << 16u, name };
}

bool ThreadID::IsDynamic() const
{
    return THREAD_DYNAMIC & value;
}

void SetCurrentThreadID(const ThreadID &thread_id)
{
    DebugLog(LogType::Debug, "SetCurrentThreadID() %u\n", thread_id.value);

    Threads::SetThreadID(thread_id);

#ifdef HYP_WINDOWS
    HRESULT set_thread_result = SetThreadDescription(
        GetCurrentThread(),
        &HYP_UTF8_TOWIDE(thread_id.name.LookupString())[0]
    );

    if (FAILED(set_thread_result)) {
        DebugLog(
            LogType::Warn,
            "Failed to set Win32 thread name for thread %s\n",
            thread_id.name.LookupString()
        );
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(thread_id.name.LookupString());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), thread_id.name.LookupString());
#endif
}

} // namespace hyperion::v2