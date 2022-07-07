#ifndef HYPERION_V2_THREADS_H
#define HYPERION_V2_THREADS_H

#include <core/lib/FlatMap.hpp>
#include <core/Thread.hpp>

#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

using ThreadMask = UInt;

enum ThreadName : ThreadMask {
    THREAD_MAIN    = 0x01,
    THREAD_RENDER  = 0x01, // for now
    THREAD_INPUT   = 0x01, // for now
    THREAD_GAME    = 0x02,
    THREAD_TERRAIN = 0x04
};

class Threads {
public:
    static const FlatMap<ThreadName, ThreadId> thread_ids;

    static void AssertOnThread(ThreadMask mask);
    static bool IsOnThread(ThreadMask mask);

    static ThreadId GetThreadId(ThreadName thread_name);
};

} // namespace hyperion::v2

#endif