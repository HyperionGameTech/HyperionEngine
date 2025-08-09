/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace threading {

class ThreadLocalStorage
{
public:
    static constexpr SizeType capacity = 64 * 1024; // 64 KiB

    ThreadLocalStorage()
    {
        m_data.SetCapacity(capacity);
    }

    ThreadLocalStorage(const ThreadLocalStorage& other) = delete;
    ThreadLocalStorage& operator=(const ThreadLocalStorage& other) = delete;
    ThreadLocalStorage(ThreadLocalStorage&& other) = delete;
    ThreadLocalStorage& operator=(ThreadLocalStorage&& other) = delete;
    ~ThreadLocalStorage() = default;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_data.Size();
    }

    HYP_FORCE_INLINE static constexpr SizeType Capacity()
    {
        return capacity;
    }

    void* Alloc(SizeType size, SizeType alignment = 0)
    {
        SizeType currentSize = m_data.Size();

        if (alignment > 0)
        {
            // Align the current size to the specified alignment
            currentSize = ((currentSize + alignment - 1) / alignment) * alignment;
        }

        if (currentSize + size >= capacity)
        {
            HYP_FAIL("ThreadLocalStorage: Allocating %zu bytes exceeds capacity of %zu bytes", currentSize + size, capacity);
        }

        void* ptr = m_data.Data() + currentSize;
        m_data.SetSize(currentSize + size);

        return ptr;
    }

    template <class T>
    T* Alloc()
    {
        void* ptr = Alloc(sizeof(T), alignof(T));

        return static_cast<T*>(ptr);
    }

private:
    ByteBuffer m_data;
};

} // namespace threading

using threading::ThreadLocalStorage;

} // namespace hyperion
