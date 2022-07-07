#include "Thread.hpp"

namespace hyperion::v2 {

void SetThreadId(const ThreadId &id)
{
#if HYP_ENABLE_THREAD_ASSERTION
    current_thread_id = id;
#endif
}

} // namespace hyperion::v2