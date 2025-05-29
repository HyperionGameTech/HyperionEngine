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

// Helper metadata for natvis navigation
enum AllocationType
{
    AT_DYNAMIC = 1,
    AT_INLINE = 2
};

template <class Derived>
struct AllocationBase
{
};

template <class Derived>
struct Allocator
{
};

struct DynamicAllocator : Allocator<DynamicAllocator>
{
    template <class T>
    struct Allocation
    {
        // These fields are used for natvis views only.
        static constexpr AllocationType allocation_type = AT_DYNAMIC;
        static constexpr bool is_dynamic = true;

        union
        {
            T* buffer;

#ifdef HYP_DEBUG_MODE // for natvis; do not use these fields.
            struct
            {
                void* buffer;
            } dynamic_allocation;

            ValueStorageArray<T, 0> storage;
#endif
        };

        SizeType capacity;

        void Allocate(SizeType count)
        {
            AssertDebug(buffer == nullptr);

            if (count == 0)
            {
                buffer = nullptr;
                capacity = 0;

                return;
            }

            buffer = static_cast<T*>(Memory::Allocate(count * sizeof(T)));
            AssertDebug(buffer != nullptr);

            capacity = count;
        }

        void Free()
        {
            if (buffer != nullptr)
            {
                Memory::Free(buffer);

                SetToInitialState();
            }
        }

        void TakeOwnership(T* begin, T* end)
        {
            AssertDebug(buffer == nullptr);

            buffer = begin;
            capacity = end - begin;
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            if constexpr (std::is_trivially_copy_constructible_v<T>)
            {
                Memory::MemCpy(buffer + offset, begin, count * sizeof(T));
            }
            else
            {
                for (SizeType i = 0; i < count; i++)
                {
                    new (buffer + offset + i) T(begin[i]);
                }
            }
        }

        void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            if constexpr (std::is_trivially_move_constructible_v<T>)
            {
                Memory::MemCpy(buffer + offset, begin, count * sizeof(T));
            }
            else
            {
                for (SizeType i = 0; i < count; i++)
                {
                    new (buffer + offset + i) T(std::move(begin[i]));
                }
            }
        }

        void InitZeroed(SizeType count, SizeType offset = 0)
        {
            AssertDebug(capacity >= count + offset);

            Memory::MemSet(buffer + offset, 0, count);
        }

        void DestructInRange(SizeType start_index, SizeType last_index)
        {
            AssertDebug(last_index <= capacity);

            if constexpr (std::is_trivially_destructible_v<T>)
            {
                return;
            }

            for (SizeType i = last_index; i > start_index;)
            {
                buffer[--i].~T();
            }
        }

        void SetToInitialState()
        {
            buffer = nullptr;
            capacity = 0;
        }

        HYP_FORCE_INLINE T* GetBuffer() const
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

    HYP_FORCE_INLINE void* Allocate(SizeType size)
    {
        return Memory::Allocate(size);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
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
        static constexpr AllocationType allocation_type = AT_INLINE;
        static constexpr SizeType capacity = Count;

        HYP_FORCE_INLINE T* GetBuffer()
        {
            return reinterpret_cast<T*>(is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetRawPointer());
        }

        HYP_FORCE_INLINE const T* GetBuffer() const
        {
            return reinterpret_cast<const T*>(is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetRawPointer());
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

            if (count <= Count)
            {
                return;
            }

            dynamic_allocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamic_allocation.SetToInitialState();
            dynamic_allocation.Allocate(count);

            is_dynamic = true;
        }

        HYP_FORCE_INLINE void Free()
        {
            if (is_dynamic)
            {
                dynamic_allocation.Free();

                SetToInitialState();
            }
        }

        void TakeOwnership(T* begin, T* end)
        {
            AssertDebug(!is_dynamic);

            dynamic_allocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamic_allocation.SetToInitialState();
            dynamic_allocation.TakeOwnership(begin, end);

            is_dynamic = true;
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            if (is_dynamic)
            {
                dynamic_allocation.InitFromRangeCopy(begin, end, offset);
            }
            else
            {
                AssertDebug(count <= Count);
                AssertDebug(offset + count <= Count);

                if constexpr (std::is_trivially_copy_constructible_v<T>)
                {
                    Memory::MemCpy(storage.GetPointer() + offset, begin, count * sizeof(T));
                }
                else
                {
                    // placement new
                    for (SizeType i = 0; i < count; i++)
                    {
                        new (storage.GetPointer() + offset + i) T(begin[i]);
                    }
                }
            }
        }

        void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
        {
            const SizeType count = end - begin;

            if (is_dynamic)
            {
                dynamic_allocation.InitFromRangeMove(begin, end, offset);
            }
            else
            {
                AssertDebug(offset + count <= Count);

                if constexpr (std::is_trivially_move_constructible_v<T>)
                {
                    Memory::MemCpy(storage.GetPointer() + offset, begin, count * sizeof(T));
                }
                else
                {
                    // placement new
                    for (SizeType i = 0; i < count; i++)
                    {
                        new (storage.GetPointer() + offset + i) T(std::move(begin[i]));
                    }
                }
            }
        }

        void InitZeroed(SizeType count, SizeType offset = 0)
        {
            if (is_dynamic)
            {
                dynamic_allocation.InitZeroed(count, offset);
            }
            else
            {
                AssertDebug(offset + count <= Count);

                Memory::MemSet(storage.GetPointer() + offset, 0, count * sizeof(T));
            }
        }

        void DestructInRange(SizeType start_index, SizeType last_index)
        {
            if (is_dynamic)
            {
                dynamic_allocation.DestructInRange(start_index, last_index);
            }
            else
            {
                AssertDebug(last_index <= Count);
                AssertDebug(start_index <= last_index);

                for (SizeType i = last_index; i > start_index;)
                {
                    storage.GetPointer()[--i].~T();
                }
            }
        }

        void SetToInitialState()
        {
            storage = ValueStorageArray<T, Count>();
            is_dynamic = false;
        }

        union
        {
            ValueStorageArray<T, Count> storage;

            typename DynamicAllocatorType::template Allocation<T> dynamic_allocation;

#ifdef HYP_DEBUG_MODE
            T* buffer; // for natvis
#endif
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