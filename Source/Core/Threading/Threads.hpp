/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Threading/Util/ThreadId.hpp>

namespace Hyperion {

namespace threading {

class ThreadBase;
class ThreadId;

enum class ThreadPriorityValue : uint32;

using ThreadMask = uint32;

// max 4 bits
enum ThreadCategory : ThreadMask
{
    THREAD_CATEGORY_NONE = 0x0,
    THREAD_CATEGORY_TASK = 0x1
};

#if HYP_DEBUG_MODE

CORE_API void AssertOnThread(ThreadMask mask, const char* message = nullptr);
CORE_API void AssertOnThread(const ThreadId& threadId, const char* message = nullptr);

#else // !HYP_DEBUG_MODE

static constexpr NoOpFunction<void> AssertOnThread;

#endif // HYP_DEBUG_MODE

CORE_API bool IsThreadInMask(const ThreadId& threadId, ThreadMask mask);
CORE_API bool IsOnThread(ThreadMask mask);
CORE_API bool IsOnThread(const ThreadId& threadId);

CORE_API ThreadBase* GetThreadById(const ThreadId& threadId);

CORE_API ThreadBase* CurrentThreadObject();

CORE_API uint32 GetCurrentThreadIndex();
CORE_API void SetCurrentThreadIndex(uint32 threadIndex);

CORE_API const ThreadId& CurrentThreadId();

CORE_API void RegisterThread(const ThreadId& id, ThreadBase* thread);
CORE_API void UnregisterThread(const ThreadId& id);
CORE_API bool IsThreadRegistered(const ThreadId& id);

CORE_API void SetCurrentThreadId(const ThreadId& id);

CORE_API void SetCurrentThreadObject(ThreadBase*);
CORE_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

CORE_API uint32 NumCores();

CORE_API void ThreadSleep(uint32 milliseconds);
CORE_API void ThreadYield();

} // namespace threading

using threading::ThreadCategory;

using threading::AssertOnThread;
using threading::CurrentThreadId;
using threading::CurrentThreadObject;
using threading::GetCurrentThreadIndex;
using threading::GetThreadById;
using threading::IsOnThread;
using threading::IsThreadInMask;
using threading::IsThreadRegistered;
using threading::NumCores;
using threading::RegisterThread;
using threading::SetCurrentThreadId;
using threading::SetCurrentThreadIndex;
using threading::SetCurrentThreadObject;
using threading::SetCurrentThreadPriority;
using threading::ThreadSleep;
using threading::ThreadYield;
using threading::UnregisterThread;

CORE_API extern const StaticThreadId g_mainThread;

CORE_API extern StaticThreadId g_simThread;
CORE_API extern StaticThreadId g_renderThread;
CORE_API extern StaticThreadId g_visThread;

} // namespace Hyperion
