/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/Types.hpp>

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
        static constexpr AllocationType allocationType = AT_DYNAMIC;
        static constexpr bool isDynamic = true;

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
                UIntPtr buffer;
                SizeType capacity;
            } dynamicAllocation;

            union
            {
                char dataBuffer[1];
            } storage;
        };

        void Allocate(SizeType count)
        {
            HYP_CORE_ASSERT(buffer == nullptr);

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
                HYP_CORE_ASSERT(buffer != nullptr, "Aligned allocation failed! Size: %zu, Alignment: %zu", count * sizeof(T), alignof(T));
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
            HYP_CORE_ASSERT(buffer == nullptr);
            HYP_CORE_ASSERT(end >= begin);

            buffer = begin;
            capacity = end - begin;
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            HYP_CORE_ASSERT(capacity >= count + offset);

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
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            HYP_CORE_ASSERT(capacity >= count + offset);

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
            HYP_CORE_ASSERT(capacity >= count + offset);

            Memory::MemSet(buffer + offset, 0, count * sizeof(T));
        }

        void DestructInRange(SizeType startIndex, SizeType lastIndex)
        {
            HYP_CORE_ASSERT(lastIndex <= capacity);

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (SizeType i = lastIndex; i > startIndex;)
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

    void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_CORE_ASSERT(size > 0);
        HYP_CORE_ASSERT(alignment > 0);

        return HYP_ALLOC_ALIGNED(size, alignment);
    }

    void Free(void* ptr)
    {
        HYP_CORE_ASSERT(ptr != nullptr);

        HYP_FREE_ALIGNED(ptr);
    }
};

template <SizeType Count, class DynamicAllocatorType = DynamicAllocator>
struct InlineAllocator : Allocator<InlineAllocator<Count, DynamicAllocatorType>>
{
    template <class T>
    struct Allocation : AllocationBase<Allocation<T>>
    {
        static constexpr AllocationType allocationType = AT_INLINE;
        static constexpr SizeType capacity = Count;

        HYP_FORCE_INLINE T* GetBuffer()
        {
            return isDynamic ? dynamicAllocation.GetBuffer() : storage.GetPointer();
        }

        HYP_FORCE_INLINE const T* GetBuffer() const
        {
            return isDynamic ? dynamicAllocation.GetBuffer() : storage.GetPointer();
        }

        HYP_FORCE_INLINE bool IsDynamic() const
        {
            return isDynamic;
        }

        HYP_FORCE_INLINE SizeType GetCapacity() const
        {
            return isDynamic ? dynamicAllocation.GetCapacity() : Count;
        }

        HYP_FORCE_INLINE void Allocate(SizeType count)
        {
            HYP_CORE_ASSERT(!isDynamic);

            if (count <= Count)
            {
                return;
            }

            dynamicAllocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamicAllocation.SetToInitialState();
            dynamicAllocation.Allocate(count);

            isDynamic = true;

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        HYP_FORCE_INLINE void Free()
        {
            if (isDynamic)
            {
                dynamicAllocation.Free();
            }

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");

            SetToInitialState();
        }

        void TakeOwnership(T* begin, T* end)
        {
            HYP_CORE_ASSERT(!isDynamic);

            dynamicAllocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamicAllocation.SetToInitialState();
            dynamicAllocation.TakeOwnership(begin, end);

            isDynamic = true;

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            if (isDynamic)
            {
                dynamicAllocation.InitFromRangeCopy(begin, end, offset);
            }
            else
            {
                HYP_CORE_ASSERT(offset + count <= Count);

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

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            if (isDynamic)
            {
                dynamicAllocation.InitFromRangeMove(begin, end, offset);
            }
            else
            {
                HYP_CORE_ASSERT(offset + count <= Count);

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

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitZeroed(SizeType count, SizeType offset = 0)
        {
            if (isDynamic)
            {
                dynamicAllocation.InitZeroed(count, offset);
            }
            else
            {
                HYP_CORE_ASSERT(offset + count <= Count);

                Memory::MemSet(storage.GetPointer() + offset, 0, count * sizeof(T));
            }

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void DestructInRange(SizeType startIndex, SizeType lastIndex)
        {
            if (isDynamic)
            {
                dynamicAllocation.DestructInRange(startIndex, lastIndex);
            }
            else
            {
                HYP_CORE_ASSERT(lastIndex <= Count);
                HYP_CORE_ASSERT(startIndex <= lastIndex);

                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (SizeType i = lastIndex; i > startIndex;)
                    {
                        storage.GetPointer()[--i].~T();
                    }
                }
            }

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void SetToInitialState()
        {
            isDynamic = false;
            storage = ValueStorage<T, Count, alignof(T)>();

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        union
        {
            ValueStorage<T, Count, alignof(T)> storage;

            typename DynamicAllocatorType::template Allocation<T> dynamicAllocation;

            T* buffer; // for natvis
        };

        // for debugging - to ensure we haven't written past the structure; // to ensure that the union is not empty and has a valid size
        uint32 magic : 31 = 0xBADA55u;
        bool isDynamic : 1 = false;
    };
};

template <SizeType Count>
struct FixedAllocator : Allocator<FixedAllocator<Count>>
{
    template <class T>
    struct Allocation : AllocationBase<Allocation<T>>
    {
        static constexpr AllocationType allocationType = AT_INLINE;
        static constexpr SizeType capacity = Count;

        HYP_FORCE_INLINE T* GetBuffer()
        {
            return storage.GetPointer();
        }

        HYP_FORCE_INLINE const T* GetBuffer() const
        {
            return storage.GetPointer();
        }

        HYP_FORCE_INLINE constexpr bool IsDynamic() const
        {
            return false;
        }

        HYP_FORCE_INLINE constexpr SizeType GetCapacity() const
        {
            return Count;
        }

        HYP_FORCE_INLINE void Allocate(SizeType count)
        {
            HYP_CORE_ASSERT(count <= Count, "Allocation size exceeds fixed capacity!");

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        HYP_FORCE_INLINE void Free()
        {
            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");

            SetToInitialState();
        }

        void TakeOwnership(T* begin, T* end)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            if constexpr (std::is_fundamental_v<T> || std::is_trivial_v<T>)
            {
                Memory::MemCpy(storage.GetPointer(), begin, count * sizeof(T));
            }
            else
            {
                // placement new
                for (SizeType i = 0; i < count; i++)
                {
                    new (storage.GetPointer()) T(begin[i]);
                }
            }

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            HYP_CORE_ASSERT(offset + count <= Count);

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

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(end >= begin);

            const SizeType count = end - begin;

            HYP_CORE_ASSERT(offset + count <= Count);

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

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void InitZeroed(SizeType count, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(offset + count <= Count);

            Memory::MemSet(storage.GetPointer() + offset, 0, count * sizeof(T));

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void DestructInRange(SizeType startIndex, SizeType lastIndex)
        {
            HYP_CORE_ASSERT(lastIndex <= Count);
            HYP_CORE_ASSERT(startIndex <= lastIndex);

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (SizeType i = lastIndex; i > startIndex;)
                {
                    storage.GetPointer()[--i].~T();
                }
            }

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        void SetToInitialState()
        {
            storage = ValueStorage<T, Count, alignof(T)>();

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        union
        {
            ValueStorage<T, Count, alignof(T)> storage;

            // The following nested union fields are unused but make natvis work correctly for arrays.
            union
            {
                UIntPtr buffer;
                SizeType capacity;
            } dynamicAllocation;

            T* buffer; // for natvis
        };

        // for debugging - to ensure we haven't written past the structure; // to ensure that the union is not empty and has a valid size
        uint32 magic : 31 = 0xBADA55u;
        bool isDynamic : 1 = false;
    };
};

} // namespace memory

using memory::Allocator;
using memory::DynamicAllocator;
using memory::FixedAllocator;
using memory::InlineAllocator;

template <class T, class AllocatorType>
using Allocation = typename AllocatorType::template Allocation<T>;

} // namespace hyperion
