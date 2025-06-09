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

/*! \brief A dynamically sized buffer, containing raw bytes. Initially has a size of zero, memory is allocated on the heap when first initialized with a non-zero size. */
HYP_STRUCT()
class HYP_API ByteBuffer
{
public:
    using AllocatorType = DynamicAllocator;

    /*! \brief Constructs an empty ByteBuffer, no memory is allocated. */
    ByteBuffer()
        : m_size(0)
    {
        m_allocation.SetToInitialState();
    }

    /*! \brief Constructs a ByteBuffer with the given size, allocating memory on the heap if \ref{count} != 0.
     *  \param count The size of the ByteBuffer in bytes. If count is zero, no memory is allocated and the ByteBuffer is set to an empty state. */
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

    /*! \brief Constructs a ByteBuffer with the given size and data, allocating memory on the heap if \ref{count} != 0 and copies the data into the buffer. */
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

    /*! \brief Constructs a ByteBuffer from a \ref{ByteView}, allocating memory on the heap if the view is not empty and copies the data into the buffer.
     *  \param view The ByteView to copy to the ByteBuffer. */
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

    /*! \brief Constructs a ByteBuffer from a \ref{ConstByteView}, allocating memory on the heap if the view is not empty and copies the data into the buffer.
     *  \param view The ConstByteView to copy to the ByteBuffer. */
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

        m_allocation.Free();

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

        if (other.m_allocation.IsDynamic())
        {
            m_allocation.TakeOwnership(other.Data(), other.Data() + m_size);

            other.m_allocation.SetToInitialState();
        }
        else
        {
            if (m_size != 0)
            {
                m_allocation.Allocate(m_size);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + m_size);
            }
        }

        other.m_size = 0;
    }

    ByteBuffer& operator=(ByteBuffer&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        const SizeType previous_size = m_size;
        const SizeType new_size = other.m_size;

        m_allocation.Free();

        if (other.m_allocation.IsDynamic())
        {
            m_allocation.TakeOwnership(other.Data(), other.Data() + new_size);

            other.m_allocation.SetToInitialState();
        }
        else
        {
            if (new_size != 0)
            {
                m_allocation.Allocate(new_size);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + new_size);
            }
        }

        m_size = new_size;

        other.m_size = 0;

        return *this;
    }

    ~ByteBuffer()
    {
        m_allocation.Free();
    }

    /*! \brief Writes \ref{count} bytes of \ref{data} to the ByteBuffer at the given \ref{offset}.
     *  \warning The ByteBuffer must have enough capacity to hold the data, otherwise an assertion will be thrown.
     *  \param count The number of bytes to write to the ByteBuffer.
     *  \param offset The offset in the ByteBuffer to write to.
     *  \param data A pointer to the data to write to the ByteBuffer. It must be a pointer to a valid memory location with at least \ref{count} bytes of data. */
    void Write(SizeType count, SizeType offset, const void* data)
    {
        if (count == 0)
        {
            return;
        }

        AssertThrow(offset + count <= m_size);

        Memory::MemCpy(Data() + offset, data, count);
    }

    /*! \brief Returns a copy of the ByteBuffer's data as an Array of ubyte.
     *  \return An Array of ubyte containing the ByteBuffer's data, copied from the ByteBuffer. */
    Array<ubyte> ToArray() const
    {
        Array<ubyte> byte_array;
        byte_array.Resize(m_size);
        Memory::MemCpy(byte_array.Data(), Data(), m_size);

        return byte_array;
    }

    /*! \brief Returns a \ref{ByteView} of the ByteBuffer's data. The ByteView will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the ByteView.
     *  \param offset The offset in the ByteBuffer to start the view from.
     *  \param size The size of the view. If size is larger than the ByteBuffer's size, it will be clamped to the ByteBuffer's size.
     *  \return A ByteView of the ByteBuffer's data. */
    ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ByteView(Data() + offset, size);
    }

    /*! \brief Returns a \ref{ConstByteView} of the ByteBuffer's data. The ConstByteView will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the ConstByteView.
     *  \param offset The offset in the ByteBuffer to start the view from.
     *  \param size The size of the view. If size is larger than the ByteBuffer's size, it will be clamped to the ByteBuffer's size.
     *  \return A ConstByteView of the ByteBuffer's data. */
    ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ConstByteView(Data() + offset, size);
    }

    /*! \brief Returns a pointer to the ByteBuffer's data. The pointer will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the pointer.
     *  \return A pointer to the ByteBuffer's data. */
    HYP_FORCE_INLINE ubyte* Data()
    {
        return m_allocation.GetBuffer();
    }

    /*! \brief Returns a pointer to the ByteBuffer's data. The pointer will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the pointer.
     *  \return A pointer to the ByteBuffer's data. */
    HYP_FORCE_INLINE const ubyte* Data() const
    {
        return m_allocation.GetBuffer();
    }

    /*! \brief Updates the ByteBuffer's data with the given data. The current data is freed and the new data is copied into the ByteBuffer, allocating memory on the heap if necessary.
     *  \param count The number of bytes to copy into the ByteBuffer.
     *  \param data A pointer to the data to copy into the ByteBuffer. If count is zero, no memory is allocated and the ByteBuffer is set to an empty state. */
    void SetData(SizeType count, const void* data)
    {
        m_allocation.Free();

        m_size = count;

        if (count == 0)
        {
            return;
        }

        m_allocation.Allocate(count);
        m_allocation.InitFromRangeCopy(reinterpret_cast<const ubyte*>(data), reinterpret_cast<const ubyte*>(data) + count);
    }

    /*! \brief Gets the current size of the ByteBuffer.
     *  \return The current size of the ByteBuffer. */
    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_size;
    }

    /*! \brief Sets the size of the ByteBuffer to the given size. If the new size is larger than the current size, the new bytes are zeroed out.
     *  If the new size is smaller than the current size, the excess bytes are freed.
     *  The current data will be copied into the newly allocated memory if the size is changed. */
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

    /*! \brief Returns the current capacity of the ByteBuffer. The capacity is the amount of memory allocated for the ByteBuffer, which may be larger than the current size.
     *  \return The current capacity of the ByteBuffer. */
    HYP_FORCE_INLINE SizeType GetCapacity() const
    {
        return m_allocation.GetCapacity();
    }

    /*! \brief Sets the capacity of the ByteBuffer to the given size. If the new capacity is larger than the current capacity, the buffer is extended and the current data is copied into the newly allocated memory.
     *  If the new capacity is smaller than the current size, the excess bytes are freed and the size is adjusted accordingly. */
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

    /*! \brief Reads a value from the ByteBuffer at the given offset. If the offset is out of bounds, the function returns false and does not modify the output.
     *  The output buffer must be large enough to hold the requested number of bytes.
     *  \param offset The offset in the ByteBuffer to read from.
     *  \param count The number of bytes to read from the ByteBuffer.
     *  \param out_values The output buffer to write the read values to.
     *  \return Returns true if the read was successful, false if the offset is out of bounds. */
    bool Read(SizeType offset, SizeType count, ubyte* out_values) const
    {
        AssertThrow(out_values != nullptr);

        const SizeType size = m_size;

        if (offset >= size || offset + count > size)
        {
            return false;
        }

        const ubyte* data = Data();

        Memory::MemCpy(out_values, data + offset, count);

        return true;
    }

    /*! \brief Reads a POD type from the ByteBuffer at the given offset. If the offset is out of bounds, the function returns false and does not modify the output.
     *  The output buffer must be a POD type and large enough to hold the requested number of bytes.
     *  \warning The type must be a POD type. A static assertion is in place to ensure this.
     *  \tparam T The type to read from the ByteBuffer, must be a POD type.
     *  \param offset The offset in the ByteBuffer to read from.
     *  \param out The output buffer to write the read value to.
     *  \return Returns true if the read was successful, false if the offset is out of bounds. */
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

        Memory::MemCpy(out, data + offset, count);

        return true;
    }

    /*! \brief Returns true if the ByteBuffer has any elements.
     *  \return True if the ByteBuffer has any elements, false otherwise. */
    HYP_FORCE_INLINE bool Any() const
    {
        return m_size != 0;
    }

    /*! \brief Returns true if the ByteBuffer has no elements.
     *  \return True if the ByteBuffer has no elements, false otherwise. */
    HYP_FORCE_INLINE bool Empty() const
    {
        return m_size == 0;
    }

    /*! \brief Returns a reference to the byte at \ref{index} in the ByteBuffer.
     *  \deprecated Use \ref{Read} to read bytes from the ByteBuffer in bulk.
     *  \warning The index must be within the bounds of the ByteBuffer or undefined behavior will occur.
     *  \param index The index of the byte to return.
     *  \return A reference to the byte at the given index. */
    HYP_DEPRECATED HYP_FORCE_INLINE ubyte& operator[](SizeType index)
    {
        return *(m_allocation.GetBuffer() + index);
    }

    /*! \brief Returns a the byte at \ref{index} in the ByteBuffer by value.
     *  \deprecated Use \ref{Read} to read bytes from the ByteBuffer in bulk.
     *  \warning The index must be within the bounds of the ByteBuffer or undefined behavior will occur.
     *  \param index The index of the byte to return.
     *  \return The byte at the given index. */
    HYP_DEPRECATED HYP_FORCE_INLINE ubyte operator[](SizeType index) const
    {
        return *(m_allocation.GetBuffer() + index);
    }

    /*! \brief Compares this ByteBuffer with another ByteBuffer for equality.
     *  Two ByteBuffers are considered equal if they have the same size and their contents are identical.
     *  \param other The other ByteBuffer to compare with.
     *  \return True if the ByteBuffers are equal, false otherwise. */
    HYP_FORCE_INLINE bool operator==(const ByteBuffer& other) const
    {
        if (m_size != other.m_size)
        {
            return false;
        }

        return Memory::MemCmp(Data(), other.Data(), m_size) == 0;
    }

    /*! \brief Compares this ByteBuffer with another ByteBuffer for inequality.
     *  Two ByteBuffers are considered unequal if they have different sizes or their contents differ.
     *  \param other The other ByteBuffer to compare with.
     *  \return True if the ByteBuffers are not equal, false otherwise. */
    HYP_FORCE_INLINE bool operator!=(const ByteBuffer& other) const
    {
        if (m_size != other.m_size)
        {
            return true;
        }

        return Memory::MemCmp(Data(), other.Data(), m_size) != 0;
    }

    /*! \brief Returns a copy of the ByteBuffer.
     *  \return A new ByteBuffer with the same size and contents as this ByteBuffer. */
    HYP_NODISCARD HYP_FORCE_INLINE ByteBuffer Copy() const
    {
        return ByteBuffer(m_size, Data());
    }

    /*! \brief Generates a HashCode based on all bytes in the buffer. Returns an empty HashCode if the ByteBuffer is empty.
     *  \return A HashCode representing the contents of the ByteBuffer. */
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