/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_THREADS_HPP
#define HYPERION_THREADS_HPP

#include <core/Defines.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/threading/Thread.hpp>

#include <Types.hpp>

namespace hyperion {
namespace threading {

enum ThreadName : ThreadMask
{
    THREAD_MAIN         = 0x01u,
    THREAD_RENDER       = THREAD_MAIN, // for now
    THREAD_GAME         = 0x04u,
    THREAD_INPUT        = THREAD_GAME,
    THREAD_RESERVED0    = 0x08u,

    THREAD_TASK_0       = 0x10u,
    THREAD_TASK_1       = 0x20u,
    THREAD_TASK_2       = 0x40u,
    THREAD_TASK_3       = 0x80u,
    THREAD_TASK_4       = 0x100u,
    THREAD_TASK_5       = 0x200u,
    THREAD_TASK_6       = 0x400u,
    THREAD_TASK_7       = 0x800u,
    THREAD_TASK_8       = 0x1000u,
    THREAD_RESERVED1    = 0x2000u,
    THREAD_RESERVED2    = 0x4000u,

    THREAD_TASK         = 0x1FF0u, //0x7ff0u, // all task threads or'd together
     
    THREAD_STATIC       = 0xFFFFu,
    THREAD_DYNAMIC      = 0xFFFFu << 16u
};

enum ThreadType : uint32
{
    THREAD_TYPE_INVALID = uint32(-1),
    THREAD_TYPE_GAME    = 0,
    THREAD_TYPE_RENDER  = 1,
    THREAD_TYPE_TASK    = 2,
    THREAD_TYPE_DYNAMIC = 3,
    THREAD_TYPE_MAX
};

class Threads
{
public:
    HYP_API static void AssertOnThread(ThreadMask mask, const char *message = nullptr);
    HYP_API static void AssertOnThread(const ThreadID &thread_id, const char *message = nullptr);
    HYP_API static bool IsThreadInMask(const ThreadID &thread_id, ThreadMask mask);
    HYP_API static bool IsOnThread(ThreadMask mask);
    HYP_API static bool IsOnThread(const ThreadID &thread_id);

    HYP_API static IThread *GetThread(ThreadID thread_id);

    HYP_API static IThread *CurrentThreadObject();

    HYP_API static ThreadID GetStaticThreadID(ThreadName thread_name);
    HYP_API static ThreadID CurrentThreadID();

    HYP_API static void SetCurrentThreadObject(IThread *);
    HYP_API static void SetCurrentThreadPriority(ThreadPriorityValue priority);

    HYP_API static ThreadType CurrentThreadType();

    HYP_API static SizeType NumCores();

    HYP_API static void Sleep(uint32 milliseconds);
};

} // namespace threading

using threading::Threads;
using threading::ThreadMask;
using threading::ThreadName;
using threading::ThreadType;

} // namespace hyperion

#endif