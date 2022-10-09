#include "Thread.hpp"
#include <Threads.hpp>

namespace hyperion::v2 {


void SetCurrentThreadID(const ThreadID &thread_id)
{
    Threads::SetThreadID(thread_id);
}

} // namespace hyperion::v2