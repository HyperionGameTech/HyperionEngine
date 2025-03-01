/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALLOCATOR_HPP
#define HYPERION_ALLOCATOR_HPP

#include <core/Defines.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <Types.hpp>

namespace hyperion {
namespace memory {

template <class Derived>
struct AllocationBase
{

};

template <class Derived>
struct Allocator
{
    // HYP_FORCE_INLINE void *Allocate(SizeType size)
    // {
    //     return static_cast<Derived *>(this)->Allocate(size);
    // }

    // HYP_FORCE_INLINE void Free(void *ptr)
    // {
    //     static_cast<Derived *>(this)->Free(ptr);
    // }
};

struct DynamicAllocator : Allocator<DynamicAllocator>
{
    template <class T>
    struct Allocation
    {
        T           *buffer;
        SizeType    capacity;

        void Allocate(SizeType count)
        {
            AssertDebug(buffer == nullptr);

            buffer = static_cast<T *>(Memory::Allocate(count * sizeof(T)));
            AssertDebug(buffer != nullptr);

            capacity = count;
        }

        void Free()
        {
            if (buffer != nullptr) {
                Memory::Free(buffer);

                SetToInitialState();
            }
        }

        void TakeOwnership(T *begin, T *end)
        {
            AssertDebug(buffer == nullptr);

            buffer = begin;
            capacity = end - begin;
        }

        void InitFromRangeCopy(const T *begin, const T *end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            for (SizeType i = 0; i < count; i++) {
                new (buffer + offset + i) T(begin[i]);
            }
        }

        void InitFromRangeMove(T *begin, T *end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            for (SizeType i = 0; i < count; i++) {
                new (buffer + offset + i) T(std::move(begin[i]));
            }
        }

        void DestructInRange(SizeType start_index, SizeType last_index)
        {
            AssertDebug(last_index <= capacity);
            
            for (SizeType i = last_index; i > start_index;) {
                buffer[--i].~T();
            }
        }

        void SetToInitialState()
        {
            buffer = nullptr;
            capacity = 0;
        }
        
        HYP_FORCE_INLINE T *GetBuffer() const
        {
            return buffer;
        }

        HYP_FORCE_INLINE bool IsDynamic() const
        {
            return buffer != nullptr;
        }

        HYP_FORCE_INLINE SizeType GetCapacity() const
        {
            return capacity;
        }
    };

    HYP_FORCE_INLINE void *Allocate(SizeType size)
    {
        return Memory::Allocate(size);
    }

    HYP_FORCE_INLINE void Free(void *ptr)
    {
        Memory::Free(ptr);
    }
};

template <SizeType Count, class DynamicAllocatorType = DynamicAllocator>
struct InlineAllocator : Allocator<InlineAllocator<Count, DynamicAllocatorType>>
{
    template <class T>
    struct Allocation : AllocationBase<Allocation<T>>
    {
        HYP_FORCE_INLINE T *GetBuffer()
        {
            return reinterpret_cast<T *>(is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetRawPointer());
        }

        HYP_FORCE_INLINE const T *GetBuffer() const
        {
            return reinterpret_cast<const T *>(is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetRawPointer());
        }

        HYP_FORCE_INLINE bool IsDynamic() const
        {
            return is_dynamic;
        }

        HYP_FORCE_INLINE SizeType GetCapacity() const
        {
            return is_dynamic ? dynamic_allocation.GetCapacity() : Count;
        }

        HYP_FORCE_INLINE void Allocate(SizeType count)
        {
            AssertDebug(!is_dynamic);

            if (count <= Count) {
                return;
            }
    
            dynamic_allocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamic_allocation.SetToInitialState();
            dynamic_allocation.Allocate(count);

            is_dynamic = true;
        }
    
        HYP_FORCE_INLINE void Free()
        {
            if (is_dynamic) {
                dynamic_allocation.Free();
        
                SetToInitialState();
            }
        }

        void TakeOwnership(T *begin, T *end)
        {
            AssertDebug(!is_dynamic);
            
            dynamic_allocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamic_allocation.SetToInitialState();
            dynamic_allocation.TakeOwnership(begin, end);

            is_dynamic = true;
        }

        void InitFromRangeCopy(const T *begin, const T *end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            if (is_dynamic) {
                dynamic_allocation.InitFromRangeCopy(begin, end, offset);
            } else {
                AssertDebug(count <= Count);
                AssertDebug(offset + count <= Count);

                for (SizeType i = 0; i < count; i++) {
                    new (storage.GetPointer() + offset + i) T(begin[i]);
                }
            }
        }

        void InitFromRangeMove(T *begin, T *end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            if (is_dynamic) {
                dynamic_allocation.InitFromRangeMove(begin, end, offset);
            } else {
                AssertDebug(count <= Count);
                AssertDebug(offset + count <= Count);

                for (SizeType i = 0; i < count; i++) {
                    new (storage.GetPointer() + offset + i) T(std::move(begin[i]));
                }
            }
        }

        void DestructInRange(SizeType start_index, SizeType last_index)
        {
            if (is_dynamic) {
                dynamic_allocation.DestructInRange(start_index, last_index);
            } else {
                AssertDebug(last_index <= Count);
                AssertDebug(start_index <= last_index);

                for (SizeType i = last_index; i > start_index;) {
                    storage.GetPointer()[--i].~T();
                }
            }
        }

        void SetToInitialState()
        {
            storage = ValueStorageArray<T, Count>();
            is_dynamic = false;
        }

        union {
            ValueStorageArray<T, Count>                             storage;

            typename DynamicAllocatorType::template Allocation<T>   dynamic_allocation;
        };

        bool is_dynamic;
    };
};

template <class T, class AllocatorType>
using Allocation = typename AllocatorType::template Allocation<T>;

} // namespace memory

using memory::Allocator;
using memory::DynamicAllocator;
using memory::InlineAllocator;

template <class T, class AllocatorType>
using Allocation = typename AllocatorType::template Allocation<T>;

} // namespace hyperion

#endif