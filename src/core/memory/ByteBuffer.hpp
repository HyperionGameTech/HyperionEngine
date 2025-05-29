/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BYTE_BUFFER_HPP
#define HYPERION_BYTE_BUFFER_HPP

#include <core/containers/Array.hpp>
#include <core/utilities/Span.hpp>
#include <core/memory/Memory.hpp>

#include <HashCode.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

namespace memory {

HYP_STRUCT()

class HYP_API ByteBuffer
{
public:
    using AllocatorType = DynamicAllocator;

    ByteBuffer()
        : m_size(0)
    {
        m_allocation.SetToInitialState();
    }

    explicit ByteBuffer(SizeType count)
        : m_size(count)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(m_size);
        m_allocation.InitZeroed(m_size);
    }

    explicit ByteBuffer(SizeType count, const void* data)
        : m_size(count)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeCopy(reinterpret_cast<const ubyte*>(data), reinterpret_cast<const ubyte*>(data) + m_size);
    }

    explicit ByteBuffer(const ByteView& view)
        : m_size(view.Size())
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeCopy(view.Begin(), view.End());
    }

    explicit ByteBuffer(const ConstByteView& view)
        : m_size(view.Size())
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeCopy(view.Begin(), view.End());
    }

    ByteBuffer(const ByteBuffer& other)
        : m_size(other.m_size)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeCopy(other.Data(), other.Data() + m_size);
    }

    ByteBuffer& operator=(const ByteBuffer& other)
    {
        if (&other == this)
        {
            return *this;
        }

        const SizeType previous_size = m_size;
        const SizeType new_size = other.m_size;

        if (previous_size != 0)
        {
            m_allocation.Free();
        }

        if (new_size != 0)
        {
            m_allocation.Allocate(new_size);
            m_allocation.InitFromRangeCopy(other.Data(), other.Data() + new_size);
        }

        m_size = new_size;

        return *this;
    }

    ByteBuffer(ByteBuffer&& other) noexcept
        : m_size(other.m_size)
    {
        m_allocation.SetToInitialState();

        if (m_size != 0)
        {
            if (other.m_allocation.IsDynamic())
            {
                m_allocation.TakeOwnership(other.Data(), other.Data() + m_size);
            }
            else
            {
                m_allocation.Allocate(m_size);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + m_size);
            }
        }

        other.m_size = 0;
        other.m_allocation.SetToInitialState();
    }

    ByteBuffer& operator=(ByteBuffer&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        const SizeType previous_size = m_size;
        const SizeType new_size = other.m_size;

        if (previous_size != 0)
        {
            m_allocation.Free();
        }

        if (new_size != 0)
        {
            if (other.m_allocation.IsDynamic())
            {
                m_allocation.TakeOwnership(other.Data(), other.Data() + new_size);
            }
            else
            {
                m_allocation.Allocate(new_size);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + new_size);
            }

            other.m_allocation.SetToInitialState();
        }

        m_size = new_size;
        other.m_size = 0;

        return *this;
    }

    ~ByteBuffer()
    {
        m_allocation.Free();
    }

    void Write(SizeType count, SizeType offset, const void* data)
    {
        if (count == 0)
        {
            return;
        }

        AssertThrow(offset + count <= m_size);

        Memory::MemCpy(Data() + offset, data, count);
    }

    /*! \brief Returns a copy of the ByteBuffer's data. */
    Array<ubyte> ToArray() const
    {
        Array<ubyte> byte_array;
        byte_array.Resize(m_size);
        Memory::MemCpy(byte_array.Data(), Data(), m_size);

        return byte_array;
    }

    /*! \brief Returns a ByteView of the ByteBuffer's data. */
    ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ByteView(Data() + offset, size);
    }

    /*! \brief Returns a ConstByteView of the ByteBuffer's data. */
    ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ConstByteView(Data() + offset, size);
    }

    HYP_FORCE_INLINE ubyte* Data()
    {
        return m_allocation.GetBuffer();
    }

    HYP_FORCE_INLINE const ubyte* Data() const
    {
        return m_allocation.GetBuffer();
    }

    /*! \brief Updates the ByteBuffer's data with the given data. */
    void SetData(SizeType count, const void* data)
    {
        m_allocation.Free();

        m_size = count;

        if (count == 0)
        {
            m_allocation.SetToInitialState();

            return;
        }

        m_allocation.Allocate(count);
        m_allocation.InitFromRangeCopy(reinterpret_cast<const ubyte*>(data), reinterpret_cast<const ubyte*>(data) + count);
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE void SetSize(SizeType new_size)
    {
        if (new_size == m_size)
        {
            return;
        }

        if (new_size > m_allocation.GetCapacity())
        {
            // Extend the buffer's capacity to ensure we have room.
            SetCapacity(new_size);
        }

        if (new_size > m_size)
        {
            // Zero out the new bytes
            m_allocation.InitZeroed(new_size - m_size, m_size);
        }

        m_size = new_size;
    }

    HYP_FORCE_INLINE SizeType GetCapacity() const
    {
        return m_allocation.GetCapacity();
    }

    HYP_FORCE_INLINE void SetCapacity(SizeType new_capacity)
    {
        const SizeType current_capacity = m_allocation.GetCapacity();

        if (new_capacity == current_capacity)
        {
            return;
        }

        Allocation<ubyte, AllocatorType> new_allocation;
        new_allocation.SetToInitialState();

        if (new_capacity != 0)
        {
            new_allocation.Allocate(new_capacity);

            const SizeType min_capacity = current_capacity <= new_capacity ? current_capacity : new_capacity;

            new_allocation.InitFromRangeMove(Data(), Data() + min_capacity);
        }

        // Chop size off if it is larger than new_capacity.
        if (new_capacity < m_size)
        {
            m_size = new_capacity;
        }

        m_allocation.Free();

        m_allocation = new_allocation;
    }

    /*! \brief Reads a value from the ByteBuffer at the given offset. */
    bool Read(SizeType offset, SizeType count, ubyte* out_values) const
    {
        AssertThrow(out_values != nullptr);

        const SizeType size = m_size;

        if (offset >= size || offset + count > size)
        {
            return false;
        }

        const ubyte* data = Data();

        for (SizeType index = offset; index < offset + count; index++)
        {
            out_values[index - offset] = data[index];
        }

        return true;
    }

    /*! \brief Reads a value from the ByteBuffer at the given offset. */
    template <class T>
    bool Read(SizeType offset, T* out) const
    {
        static_assert(is_pod_type<T>, "Must be POD type");

        AssertThrow(out != nullptr);

        constexpr SizeType count = sizeof(T);
        const SizeType size = m_size;

        if (offset >= size || offset + count > size)
        {
            return false;
        }

        const ubyte* data = Data();

        alignas(T) ubyte bytes[sizeof(T)];

        for (SizeType index = offset; index < offset + count; index++)
        {
            bytes[index - offset] = data[index];
        }

        Memory::MemCpy(out, bytes, sizeof(T));

        return true;
    }

    /*! \brief Returns true if the ByteBuffer has any elements. */
    HYP_FORCE_INLINE bool Any() const
    {
        return m_size != 0;
    }

    /*! \brief Returns true if the ByteBuffer has no elements. */
    HYP_FORCE_INLINE bool Empty() const
    {
        return m_size == 0;
    }

    HYP_DEPRECATED HYP_FORCE_INLINE ubyte& operator[](SizeType index)
    {
        return *(m_allocation.GetBuffer() + index);
    }

    HYP_DEPRECATED HYP_FORCE_INLINE ubyte operator[](SizeType index) const
    {
        return *(m_allocation.GetBuffer() + index);
    }

    HYP_FORCE_INLINE bool operator==(const ByteBuffer& other) const
    {
        if (m_size != other.m_size)
        {
            return false;
        }

        return Memory::MemCmp(Data(), other.Data(), m_size) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const ByteBuffer& other) const
    {
        return !(*this == other);
    }

    /*! \brief Returns a copy of the ByteBuffer. */
    HYP_NODISCARD HYP_FORCE_INLINE ByteBuffer Copy() const
    {
        return ByteBuffer(m_size, Data());
    }

    /*! \brief Generates a HashCode based on all bytes in the buffer. Returns an empty HashCode if the ByteBuffer is empty. */
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (Empty())
        {
            return HashCode();
        }

        return HashCode::GetHashCode(Data(), Data() + m_size);
    }

private:
    Allocation<ubyte, AllocatorType> m_allocation;
    SizeType m_size;
};

} // namespace memory

using memory::ByteBuffer;

} // namespace hyperion

#endif