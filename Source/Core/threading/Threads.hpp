/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/threading/util/ThreadId.hpp>

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

HYP_API void AssertOnThread(ThreadMask mask, const char* message = nullptr);
HYP_API void AssertOnThread(const ThreadId& threadId, const char* message = nullptr);
HYP_API bool IsThreadInMask(const ThreadId& threadId, ThreadMask mask);
HYP_API bool IsOnThread(ThreadMask mask);
HYP_API bool IsOnThread(const ThreadId& threadId);

HYP_API ThreadBase* GetThreadById(const ThreadId& threadId);

HYP_API ThreadBase* CurrentThreadObject();

HYP_API const ThreadId& CurrentThreadId();

HYP_API void RegisterThread(const ThreadId& id, ThreadBase* thread);
HYP_API void UnregisterThread(const ThreadId& id);
HYP_API bool IsThreadRegistered(const ThreadId& id);

HYP_API void SetCurrentThreadId(const ThreadId& id);

HYP_API void SetCurrentThreadObject(ThreadBase*);
HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

HYP_API uint32 NumCores();

HYP_API void ThreadSleep(uint32 milliseconds);

} // namespace threading

using threading::ThreadCategory;

using threading::AssertOnThread;
using threading::CurrentThreadId;
using threading::CurrentThreadObject;
using threading::GetThreadById;
using threading::IsOnThread;
using threading::IsThreadInMask;
using threading::IsThreadRegistered;
using threading::NumCores;
using threading::RegisterThread;
using threading::SetCurrentThreadId;
using threading::SetCurrentThreadObject;
using threading::SetCurrentThreadPriority;
using threading::ThreadSleep;
using threading::UnregisterThread;

HYP_API extern const StaticThreadId g_mainThread;

HYP_API extern StaticThreadId g_simThread;
HYP_API extern StaticThreadId g_renderThread;
HYP_API extern StaticThreadId g_visThread;

} // namespace Hyperion
