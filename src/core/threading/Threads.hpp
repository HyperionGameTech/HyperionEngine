/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_THREADS_HPP
#define HYPERION_THREADS_HPP

#include <core/Defines.hpp>

#include <core/threading/Thread.hpp>

#include <Types.hpp>

namespace hyperion {

namespace threading {

// max 4 bits
enum ThreadCategory : ThreadMask
{
    THREAD_CATEGORY_NONE = 0x0,
    THREAD_CATEGORY_TASK = 0x1
};

enum ThreadType : uint32
{
    THREAD_TYPE_INVALID = uint32(-1),
    THREAD_TYPE_GAME = 0,
    THREAD_TYPE_RENDER = 1,
    THREAD_TYPE_TASK = 2,
    THREAD_TYPE_DYNAMIC = 3,
    THREAD_TYPE_MAX
};

class HYP_API Threads
{
public:
    static void AssertOnThread(ThreadMask mask, const char* message = nullptr);
    static void AssertOnThread(const ThreadId& thread_id, const char* message = nullptr);
    static bool IsThreadInMask(const ThreadId& thread_id, ThreadMask mask);
    static bool IsOnThread(ThreadMask mask);
    static bool IsOnThread(const ThreadId& thread_id);

    static ThreadBase* GetThread(const ThreadId& thread_id);

    static ThreadBase* CurrentThreadObject();

    static const ThreadId& CurrentThreadId();

    static void RegisterThread(const ThreadId& id, ThreadBase* thread);
    static void UnregisterThread(const ThreadId& id);
    static bool IsThreadRegistered(const ThreadId& id);

    static void SetCurrentThreadId(const ThreadId& id);

    static void SetCurrentThreadObject(ThreadBase*);
    static void SetCurrentThreadPriority(ThreadPriorityValue priority);

    static uint32 NumCores();

    static void Sleep(uint32 milliseconds);
};

} // namespace threading

using threading::ThreadCategory;
using threading::Threads;
using threading::ThreadType;

HYP_API extern const StaticThreadId g_main_thread;
HYP_API extern const StaticThreadId g_render_thread;
HYP_API extern const StaticThreadId g_game_thread;

} // namespace hyperion

#endif