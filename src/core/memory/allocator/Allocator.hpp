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

#ifdef HYP_DEBUG_MODE
        union
        {
            struct
            {
                T* buffer;
                SizeType capacity;
            };

            // The following nested union fields are unused but make natvis work correctly for arrays.
            union
            {
                uintptr_t buffer;
                SizeType capacity;
            } dynamic_allocation;

            char data_buffer[1];
        };
#else
        T* buffer;

        SizeType capacity;
#endif

        void Allocate(SizeType count)
        {
            AssertDebug(buffer == nullptr);

            if (count == 0)
            {
                return;
            }

            static_assert(sizeof(T) % alignof(T) == 0, "Type T must be aligned to its size");

            if constexpr (alignof(T) <= alignof(std::max_align_t))
            {
                // Use standard allocation if alignment is not greater than max alignment
                buffer = static_cast<T*>(Memory::Allocate(count * sizeof(T)));
            }
            else
            {
                buffer = static_cast<T*>(HYP_ALLOC_ALIGNED(count * sizeof(T), alignof(T)));
                AssertDebugMsg(buffer != nullptr, "Aligned allocation failed! Size: %zu, Alignment: %zu", count * sizeof(T), alignof(T));
            }

            capacity = count;
        }

        void Free()
        {
            if (buffer != nullptr)
            {
                if constexpr (alignof(T) <= alignof(std::max_align_t))
                {
                    Memory::Free(buffer);
                }
                else
                {
                    HYP_FREE_ALIGNED(buffer);
                }
            }

            SetToInitialState();
        }

        void TakeOwnership(T* begin, T* end)
        {
            AssertDebug(buffer == nullptr);
            AssertDebug(end >= begin);

            buffer = begin;
            capacity = end - begin;
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            AssertDebug(end >= begin);

            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            if constexpr (std::is_fundamental_v<T> || std::is_trivial_v<T>)
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
            AssertDebug(end >= begin);

            const SizeType count = end - begin;

            AssertDebug(capacity >= count + offset);

            if constexpr (std::is_fundamental_v<T> || std::is_trivial_v<T>)
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

            Memory::MemSet(buffer + offset, 0, count * sizeof(T));
        }

        void DestructInRange(SizeType start_index, SizeType last_index)
        {
            AssertDebug(last_index <= capacity);

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (SizeType i = last_index; i > start_index;)
                {
                    buffer[--i].~T();
                }
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
            return is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetPointer();
        }

        HYP_FORCE_INLINE const T* GetBuffer() const
        {
            return is_dynamic ? dynamic_allocation.GetBuffer() : storage.GetPointer();
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

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
        }

        HYP_FORCE_INLINE void Free()
        {
            if (is_dynamic)
            {
                dynamic_allocation.Free();
            }

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif

            SetToInitialState();
        }

        void TakeOwnership(T* begin, T* end)
        {
            AssertDebug(!is_dynamic);

            dynamic_allocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamic_allocation.SetToInitialState();
            dynamic_allocation.TakeOwnership(begin, end);

            is_dynamic = true;

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            AssertDebug(end >= begin);

            const SizeType count = end - begin;

            if (is_dynamic)
            {
                dynamic_allocation.InitFromRangeCopy(begin, end, offset);
            }
            else
            {
                AssertDebug(offset + count <= Count);

                if constexpr (std::is_fundamental_v<T> || std::is_trivial_v<T>)
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

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
        }

        void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
        {
            AssertDebug(end >= begin);

            const SizeType count = end - begin;

            if (is_dynamic)
            {
                dynamic_allocation.InitFromRangeMove(begin, end, offset);
            }
            else
            {
                AssertDebug(offset + count <= Count);

                if constexpr (std::is_fundamental_v<T> || std::is_trivial_v<T>)
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

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
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

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
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

                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (SizeType i = last_index; i > start_index;)
                    {
                        storage.GetPointer()[--i].~T();
                    }
                }
            }

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
        }

        void SetToInitialState()
        {
            is_dynamic = false;
            storage = ValueStorageArray<T, Count, alignof(T)>();

#ifdef HYP_DEBUG_MODE
            AssertDebugMsg(magic == 0xBADA55u, "stomp detected!");
#endif
        }

        union
        {
            ValueStorageArray<T, Count, alignof(T)> storage;

            typename DynamicAllocatorType::template Allocation<T> dynamic_allocation;

#ifdef HYP_DEBUG_MODE
            T* buffer; // for natvis
#endif
        };

#ifdef HYP_DEBUG_MODE
        // for debugging - to ensure we haven't written past the structure; // to ensure that the union is not empty and has a valid size
        uint32 magic : 31 = 0xBADA55u;
#endif
        bool is_dynamic : 1 = false;
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