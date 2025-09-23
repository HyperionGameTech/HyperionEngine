/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Pimpl.hpp>
#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <type_traits>

namespace hyperion {

class HYP_API LinearPool
{
public:
    LinearPool();

    LinearPool(const LinearPool& other) = delete;
    LinearPool& operator=(const LinearPool& other) = delete;

    LinearPool(LinearPool&& other) noexcept = delete;
    LinearPool& operator=(LinearPool&& other) noexcept = delete;

    ~LinearPool();

    template <class T, class... Args>
    T* New(Args&&... args)
    {
        if constexpr (std::is_trivially_destructible_v<T>)
        {
            void* mem = Alloc(sizeof(T), alignof(T));
            HYP_CORE_ASSERT(mem != nullptr);

            return new (mem) T(std::forward<Args>(args)...);
        }
        else
        {
            void* mem = AllocWithDeleter(sizeof(T), alignof(T), &Memory::Destruct<T>);
            HYP_CORE_ASSERT(mem != nullptr);

            return new (mem) T(std::forward<Args>(args)...);
        }
    }

    void Reset();

private:
    void* Alloc(SizeType size, SizeType alignment);
    
    void* AllocWithDeleter(SizeType size, SizeType alignment, void (*destructFn)(void*));
    
    void* Alloc(
        SizeType size,
        SizeType alignment,
        void (*moveFn)(void* dst, void* src),
        void (*destructFn)(void*));

    Pimpl<class LinearPoolImpl> m_pImpl;
};

} // namespace hyperion
