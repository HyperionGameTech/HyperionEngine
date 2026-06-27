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

CORE_API ThreadAllocator& DefaultAllocatorInstanceHelper<ThreadAllocator>::operator()() const
{
    ThreadBase* currentThread = CurrentThreadObject();
    Assert(currentThread != nullptr);

    ThreadAllocator* threadAllocator = currentThread->GetThreadAllocator();
    Assert(threadAllocator != nullptr, "No ThreadAllocator for current thread!");

    return *threadAllocator;
}

} // namespace memory
} // namespace Hyperion
