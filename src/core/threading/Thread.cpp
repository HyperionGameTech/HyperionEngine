/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/Defines.hpp>
#include <util/UTF8.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(HYP_UNIX)
#include <pthread.h>
#endif

namespace hyperion {
namespace threading {

const ThreadID ThreadID::invalid = ThreadID { ~0u, NAME("InvalidThreadID") };

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

ThreadID ThreadID::Invalid()
{
    return invalid;
}

bool ThreadID::IsDynamic() const
{
    return THREAD_DYNAMIC & value;
}

bool ThreadID::IsValid() const
{
    return value != invalid.value;
}

ThreadMask ThreadID::GetMask() const
{
    return IsDynamic() ? THREAD_DYNAMIC : ThreadMask(value);
}

HYP_API void SetCurrentThreadObject(ThreadBase *thread)
{
    Threads::SetCurrentThreadObject(thread);
}

HYP_API void SetCurrentThreadID(const ThreadID &thread_id)
{
    Threads::SetCurrentThreadID(thread_id);
}

HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority)
{
    Threads::SetCurrentThreadPriority(priority);
}

} // namespace threading
} // namespace hyperion