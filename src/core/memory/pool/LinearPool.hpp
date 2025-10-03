/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Pimpl.hpp>
#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <type_traits>

namespace hyperion {
namespace memory {

/*! \brief An allocator that allocates memory as it needs, but does not allow freeing individual allocations.
    \details All memory is freed when the pool is destroyed or Reset() is called.
    This is useful for allocating many small objects that have the same lifetime as the pool itself.
    It is also useful for temporary allocations that are only needed for a short period of time.

    The LinearPool also supports types with non-trivial destructors and move constructors and are called as needed. */
class HYP_API LinearPool
{
public:
    LinearPool();

    LinearPool(const LinearPool& other) = delete;
    LinearPool& operator=(const LinearPool& other) = delete;

    LinearPool(LinearPool&& other) noexcept
        : m_pImpl(std::move(other.m_pImpl))
    {
    }

    LinearPool& operator=(LinearPool&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Reset();

        m_pImpl = std::move(other.m_pImpl);

        return *this;
    }

    ~LinearPool();

    void Reserve(SizeType size);
    void Reset();

    void* Alloc(SizeType size, SizeType alignment);

    template <class T, class... Args>
    HYP_NODISCARD T* New(Args&&... args)
    {
        static_assert(
            std::is_trivially_destructible_v<T> || !std::is_trivially_move_constructible_v<T>,
            "Types with non-trivial destructors must have non-trivial move constructors (ownership-transferring) "
            "to be safely stored in LinearPool.");

        if constexpr (std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>)
        {
            void* mem = Alloc(sizeof(T), alignof(T));
            HYP_CORE_ASSERT(mem != nullptr);

            return new (mem) T(std::forward<Args>(args)...);
        }
        else
        {
            void (*moveFn)(void*, void*) = nullptr;
            void (*destructFn)(void*) = nullptr;

            if constexpr (!std::is_trivially_move_constructible_v<T>)
            {
                moveFn = [](void* dst, void* src)
                {
                    new (dst) T(std::move(*reinterpret_cast<T*>(src)));
                };
            }

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                destructFn = &Memory::Destruct<T>;
            }

            void* mem = AllocWithDeleter(sizeof(T), alignof(T), moveFn, destructFn);
            HYP_CORE_ASSERT(mem != nullptr);

            return new (mem) T(std::forward<Args>(args)...);
        }
    }

private:
    void* AllocWithDeleter(SizeType size, SizeType alignment, void (*moveFn)(void*, void*), void (*destructFn)(void*));

    Pimpl<class LinearPoolImpl> m_pImpl;
};

template <class T, class... Args>
static inline HYP_NODISCARD T* PoolNew(LinearPool& pool, Args&&... args)
{
    return pool.New<T>(std::forward<Args>(args)...);
}

template <class T>
static inline HYP_NODISCARD T* PoolAlloc(LinearPool& pool)
{
    return reinterpret_cast<T*>(pool.Alloc(sizeof(T), alignof(T)));
}

} // namespace memory

using memory::LinearPool;
using memory::PoolAlloc;
using memory::PoolNew;

} // namespace hyperion
