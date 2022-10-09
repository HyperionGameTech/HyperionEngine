#ifndef HYPERION_V2_THREADS_H
#define HYPERION_V2_THREADS_H

#include <core/lib/FlatMap.hpp>
#include <core/Thread.hpp>

#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

using ThreadMask = UInt;

enum ThreadName : ThreadMask
{
    THREAD_MAIN = 0x01,
    THREAD_RENDER = THREAD_MAIN, // for now
    THREAD_GAME = 0x04,
    THREAD_INPUT = THREAD_GAME,
    THREAD_TERRAIN = 0x08,

    THREAD_TASK_0 = 0x10,
    THREAD_TASK_1 = 0x20,
    THREAD_TASK_2 = 0x40,
    THREAD_TASK_3 = 0x80,
    THREAD_TASK_4 = 0x100,
    THREAD_TASK_5 = 0x200,
    THREAD_TASK_6 = 0x400,
    THREAD_TASK_7 = 0x800,

    THREAD_TASK = 0xFF0,

    THREAD_PHYSICS = THREAD_GAME // for now
};

class Threads
{
public:
    static const FlatMap<ThreadName, ThreadID> thread_ids;

    static void AssertOnThread(ThreadMask mask);
    static void AssertOnThread(const ThreadID &thread_id);
    static bool IsThreadInMask(const ThreadID &thread_id, ThreadMask mask);
    static bool IsOnThread(ThreadMask mask);
    static bool IsOnThread(const ThreadID &thread_id);
    static const ThreadID &GetThreadID(ThreadName thread_name);
    static const ThreadID &CurrentThreadID();
    static void SetThreadID(const ThreadID &thread_id);
};

} // namespace hyperion::v2

#endif