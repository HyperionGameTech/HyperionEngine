#include "Thread.hpp"
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

const ThreadID ThreadID::invalid = ThreadID { ~0u, "INVALID" };

void SetCurrentThreadID(const ThreadID &thread_id)
{
    Threads::SetThreadID(thread_id);

#ifdef HYP_WINDOWS
    HRESULT set_thread_result = SetThreadDescription(
        GetCurrentThread(),
        &utf::ToWide(thread_id.name.Data())[0]
    );

    if (FAILED(set_thread_result)) {
        DebugLog(
            LogType::Warn,
            "Failed to set Win32 thread name for thread %s\n",
            thread_id.name.Data()
        );
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(thread_id.name.Data());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), thread_id.name.Data());
#endif
}

} // namespace hyperion::v2