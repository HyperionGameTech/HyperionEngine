/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/allocator/Allocator.hpp>

namespace Hyperion {

namespace threading {

class ThreadBase;
class ThreadLocalStorage;

CORE_API extern ThreadBase* CurrentThreadObject();

} // namespace threading

namespace memory {

class Pool;

template <class InnerAllocator, void (*InitInnerAllocatorFunction)(void*)>
struct TThreadAllocator : Allocator<TThreadAllocator<InnerAllocator, InitInnerAllocatorFunction>>
{
    static constexpr uint32 maxAlign = InnerAllocator::maxAlign;

    static thread_local InnerAllocator* s_innerAllocator;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    HYP_FORCE_INLINE void* Allocate(size_t size, size_t alignment)
    {
        if (HYP_UNLIKELY(!s_innerAllocator))
        {
            s_innerAllocator = ThreadLocalAlloc<threading::ThreadBase, InnerAllocator>([]()
                {
                    if (s_innerAllocator != nullptr)
                    {
                        s_innerAllocator->~InnerAllocator();
                    }
                });

            HYP_CORE_ASSERT(s_innerAllocator != nullptr);

            InitInnerAllocatorFunction(s_innerAllocator);
        }

        return s_innerAllocator->Allocate(size, alignment);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        HYP_CORE_ASSERT(s_innerAllocator != nullptr, "Free from wrong thread or freeing a pointer not allocated from this allocator!!");

        s_innerAllocator->Free(ptr);
    }

private:
    template <class TThreadBase, class T>
    static T* ThreadLocalAlloc(void (*freeFunction)(void))
    {
        TThreadBase* currentThread = reinterpret_cast<TThreadBase*>(threading::CurrentThreadObject());
        HYP_CORE_ASSERT(currentThread != nullptr);

        T* ptr = ThreadLocalAlloc2<TThreadBase, threading::ThreadLocalStorage, T>(currentThread);

        if (ptr)
        {
            if (freeFunction)
            {
                currentThread->AddOnExitCallback(freeFunction);
            }

            return ptr;
        }

        return nullptr;
    }

    template <class TThreadBase, class TThreadLocalStorage, class T>
    static T* ThreadLocalAlloc2(TThreadBase* currentThread)
    {
        return reinterpret_cast<TThreadLocalStorage&>(currentThread->GetTLS()).template Allocate<T>();
    }
};

template <class InnerAllocator, void (*InitInnerAllocatorFunction)(void*)>
thread_local InnerAllocator* TThreadAllocator<InnerAllocator, InitInnerAllocatorFunction>::s_innerAllocator = nullptr;

extern void InitThreadAllocatorPool(void*);
using ThreadAllocator = TThreadAllocator<memory::Pool, &InitThreadAllocatorPool>;

} // namespace memory

using memory::ThreadAllocator;
using memory::TThreadAllocator;

} // namespace Hyperion
