/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_THREAD_LOCAL_STORAGE_HPP
#define HYPERION_THREAD_LOCAL_STORAGE_HPP

#include <core/Defines.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <Types.hpp>

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
        SizeType current_size = m_data.Size();

        if (alignment > 0)
        {
            // Align the current size to the specified alignment
            current_size = ((current_size + alignment - 1) / alignment) * alignment;
        }

        if (current_size + size >= capacity)
        {
            HYP_FAIL("ThreadLocalStorage: Allocating %llu bytes exceeds capacity of %llu bytes", current_size + size, capacity);
        }

        void* ptr = m_data.Data() + current_size;
        m_data.SetSize(current_size + size);

        return ptr;
    }

    template <class T>
    T* Alloc()
    {
        static_assert(std::is_trivial_v<T>, "T must be a trivial type");

        void* ptr = Alloc(sizeof(T), alignof(T));

        return static_cast<T*>(ptr);
    }

private:
    ByteBuffer m_data;
};

} // namespace threading

using threading::ThreadLocalStorage;

} // namespace hyperion

#endif