/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Util/ThreadId.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>

namespace Hyperion {
namespace memory {

thread_local ThreadAllocator* t_threadAllocator;

CORE_API ThreadAllocator& DefaultAllocatorInstanceHelper<ThreadAllocator>::operator()() const
{
    if (HYP_LIKELY(t_threadAllocator != nullptr))
    {
        return *t_threadAllocator;
    }

    ThreadBase* currentThread = CurrentThreadObject();
    Assert(currentThread != nullptr);

    ThreadAllocator* threadAllocator = currentThread->GetThreadAllocator();
    Assert(threadAllocator != nullptr, "No ThreadAllocator for current thread!");

    return *(t_threadAllocator = threadAllocator);
}

} // namespace memory
} // namespace Hyperion
