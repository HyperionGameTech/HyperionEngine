/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/memory/Memory.hpp>

#include <Core/utilities/ValueStorage.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace memory {

class Pool;

template <class T>
concept HasDefaultAllocatorInstance = std::is_default_constructible_v<T>;

template <HasDefaultAllocatorInstance AllocatorType>
AllocatorType* GetDefaultAllocatorInstance();

template <class T, class T2 = void>
struct DefaultAllocatorInstanceHelper;

template <class T>
struct DefaultAllocatorInstanceHelper<T>
{
    T& operator()() const;
};

template <HasDefaultAllocatorInstance AllocatorType>
struct GetAllocatorInstanceHelper
{
    using Type = AllocatorType;

    static AllocatorType* Get()
    {
        return GetDefaultAllocatorInstance<AllocatorType>();
    }
};

// Helper metadata for natvis navigation
enum AllocationType : uint32
{
    AT_DYNAMIC = 1,
    AT_INLINE = 2
};

template <class T>
struct AllocationBase
{
};

template <class T>
struct DynamicAllocationBase : AllocationBase<T>
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

    template <class AllocatorType>
    HYP_FORCE_INLINE void Allocate(AllocatorType* allocator, SizeType count, SizeType alignment = alignof(T))
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

    template <class AllocatorType>
    HYP_FORCE_INLINE void Free(AllocatorType* allocator)
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
            Memory::Copy(buffer + offset, begin, count * sizeof(T));
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
            Memory::Copy(buffer + offset, begin, count * sizeof(T));
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

        Memory::Zero(buffer + offset, count * sizeof(T));
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

struct DynamicAllocator : Allocator<DynamicAllocator>
{
    static constexpr uint32 maxAlign = ~0u;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
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
    static constexpr uint32 maxAlign = ~0u;

    template <class T>
    struct Allocation : AllocationBase<T>
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
                    Memory::Copy(storage.GetPointer() + offset, begin, count * sizeof(T));
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
                    Memory::Copy(storage.GetPointer() + offset, begin, count * sizeof(T));
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

                Memory::Fill(storage.GetPointer() + offset, 0, count * sizeof(T));
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
    static constexpr uint32 maxAlign = ~0u;

    template <class T>
    struct Allocation : AllocationBase<T>
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
                Memory::Copy(storage.GetPointer(), begin, count * sizeof(T));
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
                Memory::Copy(storage.GetPointer() + offset, begin, count * sizeof(T));
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
                Memory::Copy(storage.GetPointer() + offset, begin, count * sizeof(T));
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

            Memory::Fill(storage.GetPointer() + offset, 0, count * sizeof(T));

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

/*! \brief Binds a pointer-to-pointer of a given AllocatorType, allowing the instance to be passed as an allocator to other structures. */
template <class AllocatorType, AllocatorType** GlobalInstance = nullptr>
struct AllocatorInstance : Allocator<AllocatorInstance<AllocatorType, GlobalInstance>>
{
    static AllocatorType** s_globalInstance;

    static constexpr uint32 maxAlign = AllocatorType::maxAlign;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    AllocatorInstance()
        : pAllocator(nullptr)
    {
        if constexpr (GlobalInstance != nullptr)
        {
            pAllocator = *GlobalInstance;
            HYP_CORE_ASSERT(pAllocator != nullptr);
        }
        else
        {
            HYP_CORE_ASSERT(false, "No global allocator provided for AllocatorInstance!");
        }
    }

    explicit AllocatorInstance(AllocatorType* pAllocator)
        : pAllocator(pAllocator)
    {
    }

    HYP_FORCE_INLINE void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_CORE_ASSERT(pAllocator != nullptr);

        HYP_CORE_ASSERT(size > 0);
        HYP_CORE_ASSERT(alignment > 0);

        return pAllocator->Allocate(size, alignment);
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
Derived& DefaultAllocatorInstanceHelper<Derived>::operator()() const
{
    static Derived s_instance;
    return s_instance;
}

template <class AllocatorType, AllocatorType** GlobalInstance>
AllocatorType** AllocatorInstance<AllocatorType, GlobalInstance>::s_globalInstance = GlobalInstance;

template <class AllocatorType>
struct GetAllocatorInstanceHelper<AllocatorInstance<AllocatorType>>
{
    using Type = AllocatorInstance<AllocatorType>;

    static AllocatorType* Get()
    {
        return *AllocatorInstance<AllocatorType>::s_globalInstance;
    }
};

template <HasDefaultAllocatorInstance AllocatorType>
static inline auto GetAllocatorInstance() -> typename GetAllocatorInstanceHelper<AllocatorType>::Type*
{
    return GetAllocatorInstanceHelper<AllocatorType>::Get();
}

template <HasDefaultAllocatorInstance AllocatorType>
AllocatorType* GetDefaultAllocatorInstance()
{
    return &DefaultAllocatorInstanceHelper<AllocatorType> {}();
}

} // namespace memory

using memory::AllocationBase;
using memory::Allocator;
using memory::AllocatorInstance;
using memory::DynamicAllocationBase;
using memory::DynamicAllocator;
using memory::FixedAllocator;
using memory::InlineAllocator;

using memory::GetDefaultAllocatorInstance;
using memory::HasDefaultAllocatorInstance;

template <class T, class AllocatorType>
using Allocation = typename AllocatorType::template Allocation<T>;

} // namespace Hyperion
