/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace memory {

class Pool;

template <class T>
T& GetDefaultAllocatorInstance();

// Helper metadata for natvis navigation
enum AllocationType : uint32
{
    AT_DYNAMIC = 1,
    AT_INLINE = 2
};

template <class Derived>
struct AllocationBase
{
};

// Allocator interface (CRTP)
template <class Derived>
struct Allocator
{
    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        return static_cast<Derived*>(this)->Allocate(size, alignment);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        static_cast<Derived*>(this)->Free(ptr);
    }
};

template <class T, class T2 = void>
struct DefaultAllocatorInstanceHelper;

template <class Derived>
struct DefaultAllocatorInstanceHelper<Derived, typename std::enable_if_t<std::is_base_of_v<Allocator<Derived>, Derived>>>
{
    Derived& operator()() const;
};

template <class T>
T& GetDefaultAllocatorInstance()
{
    return DefaultAllocatorInstanceHelper<T> {}();
}

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

        HYP_FORCE_INLINE void Allocate(DynamicAllocator* allocator, SizeType count, SizeType alignment = alignof(T))
        {
            HYP_CORE_ASSERT(buffer == nullptr);

            if (count == 0)
            {
                return;
            }

            HYP_CORE_ASSERT(alignment >= alignof(T));

            buffer = static_cast<T*>(allocator->Allocate(sizeof(T) * count, alignment));
            HYP_CORE_ASSERT(buffer != nullptr);

            capacity = count;
        }

        HYP_FORCE_INLINE void Free(DynamicAllocator* allocator)
        {
            if (buffer != nullptr)
            {
                allocator->Free(buffer);
            }

            SetToInitialState();
        }

        HYP_FORCE_INLINE void TakeOwnership(T* begin, T* end)
        {
            HYP_CORE_ASSERT(buffer == nullptr);
            HYP_CORE_ASSERT(end >= begin);

            buffer = begin;
            capacity = end - begin;
        }

        HYP_FORCE_INLINE void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
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

        HYP_FORCE_INLINE void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
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

        HYP_FORCE_INLINE void InitZeroed(SizeType count, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(capacity >= count + offset);

            Memory::MemSet(buffer + offset, 0, count * sizeof(T));
        }

        HYP_FORCE_INLINE void DestructInRange(SizeType startIndex, SizeType lastIndex)
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

        HYP_FORCE_INLINE void SetToInitialState()
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

    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_CORE_ASSERT(size > 0);
        HYP_CORE_ASSERT(alignment > 0);

        return std::malloc(size);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        HYP_CORE_ASSERT(ptr != nullptr);

        std::free(ptr);
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

        HYP_FORCE_INLINE void Allocate(InlineAllocator<Count, DynamicAllocatorType>* allocator, SizeType count, SizeType alignment = alignof(T))
        {
            HYP_CORE_ASSERT(!isDynamic);

            if (count <= Count)
            {
                return;
            }

            dynamicAllocation = typename DynamicAllocatorType::template Allocation<T>();
            dynamicAllocation.SetToInitialState();
            dynamicAllocation.Allocate(&allocator->dynamicAllocator, count, alignment);

            isDynamic = true;

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        HYP_FORCE_INLINE void Free(InlineAllocator<Count, DynamicAllocatorType>* allocator)
        {
            if (isDynamic)
            {
                dynamicAllocation.Free(&allocator->dynamicAllocator);
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

    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        // Inline allocations should be handled by the Allocation struct itself
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        // Inline allocations should be handled by the Allocation struct itself
        HYP_NOT_IMPLEMENTED();
    }

    DynamicAllocatorType dynamicAllocator;
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

        HYP_FORCE_INLINE void Allocate(FixedAllocator<Count>* allocator, SizeType count, SizeType alignment = alignof(T))
        {
            HYP_CORE_ASSERT(count <= Count, "Allocation size exceeds fixed capacity!");

            HYP_CORE_ASSERT(magic == 0xBADA55u, "stomp detected!");
        }

        HYP_FORCE_INLINE void Free(FixedAllocator<Count>* allocator)
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

    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        // Fixed allocations should be handled by the Allocation struct itself
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        // Fixed allocations should be handled by the Allocation struct itself
        HYP_NOT_IMPLEMENTED();
    }
};

template <class AllocatorType, AllocatorType** GlobalAllocator = nullptr>
struct TAllocator : Allocator<TAllocator<AllocatorType, GlobalAllocator>>
{
    template <class T>
    struct Allocation : AllocationBase<Allocation<T>>
    {
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

        HYP_FORCE_INLINE void Allocate(TAllocator* allocator, SizeType count, SizeType alignment = alignof(T))
        {
            HYP_CORE_ASSERT(buffer == nullptr);

            if (count == 0)
            {
                return;
            }

            HYP_CORE_ASSERT(alignment >= alignof(T));

            buffer = static_cast<T*>(allocator->Allocate(count * sizeof(T), alignment));
            HYP_CORE_ASSERT(buffer != nullptr, "Pool allocation failed! Size: %zu, Alignment: %zu", count * sizeof(T), alignment);

            capacity = count;
        }

        HYP_FORCE_INLINE void Free(TAllocator* allocator)
        {
            if (buffer != nullptr)
            {
                allocator->Free(buffer);
            }

            SetToInitialState();
        }

        HYP_FORCE_INLINE void TakeOwnership(T* begin, T* end)
        {
            HYP_CORE_ASSERT(buffer == nullptr);
            HYP_CORE_ASSERT(end >= begin);

            buffer = begin;
            capacity = end - begin;
        }

        HYP_FORCE_INLINE void InitFromRangeCopy(const T* begin, const T* end, SizeType offset = 0)
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

        HYP_FORCE_INLINE void InitFromRangeMove(T* begin, T* end, SizeType offset = 0)
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

        HYP_FORCE_INLINE void InitZeroed(SizeType count, SizeType offset = 0)
        {
            HYP_CORE_ASSERT(capacity >= count + offset);

            Memory::MemSet(buffer + offset, 0, count * sizeof(T));
        }

        HYP_FORCE_INLINE void DestructInRange(SizeType startIndex, SizeType lastIndex)
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

        HYP_FORCE_INLINE void SetToInitialState()
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

    TAllocator()
        : pAllocator(nullptr)
    {
        if constexpr (GlobalAllocator != nullptr)
        {
            pAllocator = *GlobalAllocator;
            HYP_CORE_ASSERT(pAllocator != nullptr);
        }
        else
        {
            HYP_CORE_ASSERT(false, "No global allocator provided for TAllocator!");
        }
    }

    explicit TAllocator(AllocatorType* pAllocator)
        : pAllocator(pAllocator)
    {
    }

    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_CORE_ASSERT(pAllocator != nullptr);

        HYP_CORE_ASSERT(size > 0);
        HYP_CORE_ASSERT(alignment > 0);

        return pAllocator->Alloc(size, alignment);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        HYP_CORE_ASSERT(pAllocator != nullptr);

        HYP_CORE_ASSERT(ptr != nullptr);

        pAllocator->Free(ptr);
    }

    AllocatorType* pAllocator;
};

template <class Derived>
Derived& DefaultAllocatorInstanceHelper<Derived, typename std::enable_if_t<std::is_base_of_v<Allocator<Derived>, Derived>>>::operator()() const
{
    static Derived s_instance;
    return s_instance;
}

} // namespace memory

using memory::Allocator;
using memory::DynamicAllocator;
using memory::FixedAllocator;
using memory::GetDefaultAllocatorInstance;
using memory::InlineAllocator;
using memory::TAllocator;

template <class T, class AllocatorType>
using Allocation = typename AllocatorType::template Allocation<T>;

} // namespace hyperion
