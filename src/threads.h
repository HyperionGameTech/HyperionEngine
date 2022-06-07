#ifndef HYPERION_V2_THREADS_H
#define HYPERION_V2_THREADS_H

#include <core/lib/flat_map.h>
#include <core/thread.h>

#include <system/debug.h>

#include <types.h>

namespace hyperion::v2 {

using ThreadMask = uint;

enum ThreadName : ThreadMask {
    THREAD_MAIN   = 0x01,
    THREAD_RENDER = 0x01, // for now
    THREAD_GAME   = 0x02
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