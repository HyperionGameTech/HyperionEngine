/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>

#include <Core/utilities/Span.hpp>
#include <Core/utilities/ByteUtil.hpp>

#include <Core/memory/Memory.hpp>

namespace Hyperion {
namespace memory {

template <class Allocator, size_t BufferAlignment = 16>
class TByteBuffer
{
    template <class OtherAllocator, size_t OtherBufferAlignment>
    friend class TByteBuffer;

public:
    using AllocatorType = Allocator;

    /*! \brief Constructs an empty ByteBuffer, no memory is allocated. */
    TByteBuffer()
        : m_size(0)
    {
        m_allocation.SetToInitialState();
    }

    /*! \brief Constructs a ByteBuffer with the given size, allocating memory on the heap if \p count != 0.
     *  \param count The size of the ByteBuffer in bytes. If count is zero, no memory is allocated and the ByteBuffer is set to an empty state.
     *  \param zeroize If true, the memory is initialized to zero. */
    explicit TByteBuffer(size_t count, bool zeroize = true)
        : m_size(count)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);

        if (zeroize)
        {
            m_allocation.InitZeroed(m_size);
        }
    }

    /*! \brief Constructs a ByteBuffer with the given size and data, allocating memory on the heap if \p count != 0 and copies the data into the buffer. */
    explicit TByteBuffer(size_t count, const void* data)
        : m_size(count)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(reinterpret_cast<const ubyte*>(data), reinterpret_cast<const ubyte*>(data) + m_size);
    }

    /*! \brief Constructs a ByteBuffer from \p view, allocating memory on the heap if the view is not empty and copies the data into the buffer.
     *  \param view The ByteView to copy to the ByteBuffer. */
    explicit TByteBuffer(const ByteView& view)
        : m_size(view.Size())
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(view.Begin(), view.End());
    }

    /*! \brief Constructs a ByteBuffer from a \ref ConstByteView, allocating memory on the heap if the view is not empty and copies the data into the buffer.
     *  \param view The ConstByteView to copy to the ByteBuffer. */
    explicit TByteBuffer(const ConstByteView& view)
        : m_size(view.Size())
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(view.Begin(), view.End());
    }

    TByteBuffer(const TByteBuffer& other)
        : m_size(other.m_size)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(other.Data(), other.Data() + m_size);
    }

    template <class OtherAllocator>
    TByteBuffer(const TByteBuffer<OtherAllocator>& other)
        : m_size(other.m_size)
    {
        m_allocation.SetToInitialState();

        if (m_size == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(other.Data(), other.Data() + m_size);
    }

    TByteBuffer& operator=(const TByteBuffer& other)
    {
        if (&other == this)
        {
            return *this;
        }

        const size_t newSize = other.m_size;

        m_allocation.Free(GetAllocator());

        if (newSize != 0)
        {
            m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(newSize, BufferAlignment), /* alignment */ BufferAlignment);
            m_allocation.InitFromRangeCopy(other.Data(), other.Data() + newSize);
        }

        m_size = newSize;

        return *this;
    }

    template <class OtherAllocator>
    TByteBuffer& operator=(const TByteBuffer<OtherAllocator>& other)
    {
        if (&other == this)
        {
            return *this;
        }

        const size_t newSize = other.m_size;

        m_allocation.Free(GetAllocator());

        if (newSize != 0)
        {
            m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(newSize, BufferAlignment), /* alignment */ BufferAlignment);
            m_allocation.InitFromRangeCopy(other.Data(), other.Data() + newSize);
        }

        m_size = newSize;

        return *this;
    }

    TByteBuffer(TByteBuffer<AllocatorType>&& other) noexcept
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
                m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(m_size, BufferAlignment), /* alignment */ BufferAlignment);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + m_size);
            }

            other.m_allocation.Free(GetAllocator());
        }

        other.m_size = 0;
    }

    TByteBuffer& operator=(TByteBuffer<AllocatorType>&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        const size_t newSize = other.m_size;

        m_allocation.Free(GetAllocator());

        if (other.m_allocation.IsDynamic())
        {
            m_allocation.TakeOwnership(other.Data(), other.Data() + newSize);

            other.m_allocation.SetToInitialState();
        }
        else
        {
            if (newSize != 0)
            {
                m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(newSize, BufferAlignment), /* alignment */ BufferAlignment);
                m_allocation.InitFromRangeMove(other.Data(), other.Data() + newSize);
            }

            other.m_allocation.Free(GetAllocator());
        }

        m_size = newSize;

        other.m_size = 0;

        return *this;
    }

    ~TByteBuffer()
    {
        m_allocation.Free(GetAllocator());
    }

    /*! \brief Writes \p count bytes of \p data to the ByteBuffer at the given \p offset.
     *  \warning The ByteBuffer must have enough capacity to hold the data, otherwise an assertion will be thrown.
     *  \param count The number of bytes to write to the ByteBuffer.
     *  \param offset The offset in the ByteBuffer to write to.
     *  \param data A pointer to the data to write to the ByteBuffer. It must be a pointer to a valid memory location with at least \p count bytes of data. */
    HYP_FORCE_INLINE void Write(size_t count, size_t offset, const void* data)
    {
        if (count == 0)
        {
            return;
        }

        HYP_CORE_ASSERT(offset + count <= m_size);

        Memory::Copy(Data() + offset, data, count);
    }

    /*! \brief Returns a copy of the ByteBuffer's data as an Array of ubyte.
     *  \return An Array of ubyte containing the ByteBuffer's data, copied from the ByteBuffer. */
    HYP_FORCE_INLINE Array<ubyte> ToArray() const
    {
        Array<ubyte> byteArray;

        if (m_size != 0)
        {
            byteArray.Resize(m_size);
            Memory::Copy(byteArray.Data(), Data(), m_size);
        }

        return byteArray;
    }

    /*! \brief Returns a \ref ByteView of the ByteBuffer's data. The ByteView will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the ByteView.
     *  \param offset The offset in the ByteBuffer to start the view from.
     *  \param size The size of the view. If size is larger than the ByteBuffer's size, it will be clamped to the ByteBuffer's size.
     *  \return A ByteView of the ByteBuffer's data. */
    HYP_FORCE_INLINE ByteView ToByteView(size_t offset = 0, size_t size = ~0ull)
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ByteView(Data() + offset, size);
    }

    /*! \brief Returns a \ref ConstByteView of the ByteBuffer's data. The ConstByteView will point to the same data as the ByteBuffer, so changes to the ByteBuffer will be reflected in the ConstByteView.
     *  \param offset The offset in the ByteBuffer to start the view from.
     *  \param size The size of the view. If size is larger than the ByteBuffer's size, it will be clamped to the ByteBuffer's size.
     *  \return A ConstByteView of the ByteBuffer's data. */
    HYP_FORCE_INLINE ConstByteView ToByteView(size_t offset = 0, size_t size = ~0ull) const
    {
        if (size > m_size)
        {
            size = m_size;
        }

        return ConstByteView(Data() + offset, size);
    }

    /*! \brief Returns a pointer to the ByteBuffer's data  */
    HYP_FORCE_INLINE ubyte* Data()
    {
        return m_allocation.GetBuffer();
    }

    /*! \brief Returns a pointer to the ByteBuffer's data  */
    HYP_FORCE_INLINE const ubyte* Data() const
    {
        return m_allocation.GetBuffer();
    }

    /*! \brief Updates the ByteBuffer's data with the given data. The current data is freed and the new data is copied into the ByteBuffer, allocating memory on the heap if necessary.
     *  \param count The number of bytes to copy into the ByteBuffer.
     *  \param data A pointer to the data to copy into the ByteBuffer. If count is zero, no memory is allocated and the ByteBuffer is set to an empty state. */
    void SetData(size_t count, const void* data)
    {
        m_allocation.Free(GetAllocator());

        m_size = count;

        if (count == 0)
        {
            return;
        }

        m_allocation.Allocate(GetAllocator(), ByteUtil::AlignAs(count, BufferAlignment), /* alignment */ BufferAlignment);
        m_allocation.InitFromRangeCopy(reinterpret_cast<const ubyte*>(data), reinterpret_cast<const ubyte*>(data) + count);
    }

    /*! \brief Gets the current size of the ByteBuffer in bytes.
     *  \return The current size of the ByteBuffer. */
    HYP_FORCE_INLINE size_t Size() const
    {
        return m_size;
    }

    /*! \brief Sets the size of the ByteBuffer to the given size. If the new size is larger than the current size, the new bytes are zeroed out.
     *  If the new size is smaller than the current size, the excess bytes are freed.
     *  The current data will be copied into the newly allocated memory if the size is changed.
     * \param newSize The new size of the ByteBuffer in bytes.
     * \param zeroize If true, the new bytes are zeroed out. */
    void SetSize(size_t newSize, bool zeroize = true)
    {
        if (newSize == m_size)
        {
            return;
        }

        if (newSize > m_allocation.GetCapacity())
        {
            // Extend the buffer's capacity to ensure we have room.
            SetCapacity(newSize);
        }

        if (newSize > m_size && zeroize)
        {
            // Zero out the new bytes
            m_allocation.InitZeroed(newSize - m_size, m_size);
        }

        m_size = newSize;
    }

    /*! \brief Returns the current capacity of the ByteBuffer. The capacity is the amount of memory allocated for the ByteBuffer, which may be larger than the current size.
     *  \return The current capacity of the ByteBuffer. */
    HYP_FORCE_INLINE size_t GetCapacity() const
    {
        return m_allocation.GetCapacity();
    }

    /*! \brief Sets the capacity of the ByteBuffer to the given size. If the new capacity is larger than the current capacity, the buffer is extended and the current data is copied into the newly allocated memory.
     *  If the new capacity is smaller than the current size, the excess bytes are freed and the size is adjusted accordingly. */
    void SetCapacity(size_t newCapacity)
    {
        const size_t currentCapacity = m_allocation.GetCapacity();

        if (newCapacity == currentCapacity)
        {
            return;
        }

        Allocation<ubyte, AllocatorType> newAllocation;
        newAllocation.SetToInitialState();

        if (newCapacity != 0)
        {
            newAllocation.Allocate(GetAllocator(), ByteUtil::AlignAs(newCapacity, BufferAlignment), /* alignment */ BufferAlignment);

            const size_t minCapacity = currentCapacity <= newCapacity ? currentCapacity : newCapacity;

            newAllocation.InitFromRangeMove(Data(), Data() + minCapacity);
        }

        // Chop size off if it is larger than newCapacity.
        if (newCapacity < m_size)
        {
            m_size = newCapacity;
        }

        m_allocation.Free(GetAllocator());

        m_allocation = newAllocation;

        HYP_CORE_ASSERT(GetCapacity() >= newCapacity);
    }

    /*! \brief Reads a value from the ByteBuffer at the given offset. If the offset is out of bounds, the function returns false and does not modify the output.
     *  The output buffer must be large enough to hold the requested number of bytes.
     *  \param offset The offset in the ByteBuffer to read from.
     *  \param count The number of bytes to read from the ByteBuffer.
     *  \param outValues The output buffer to write the read values to.
     *  \return Returns true if the read was successful, false if the offset is out of bounds. */
    bool Read(size_t offset, size_t count, ubyte* outValues) const
    {
        HYP_CORE_ASSERT(outValues != nullptr);

        const size_t size = m_size;

        if (offset >= size || offset + count > size)
        {
            return false;
        }

        const ubyte* data = Data();

        Memory::Copy(outValues, data + offset, count);

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
    bool Read(size_t offset, T* out) const
    {
        static_assert(is_pod_type_v<T>, "Must be POD type");

        HYP_CORE_ASSERT(out != nullptr);

        constexpr size_t count = sizeof(T);
        const size_t size = m_size;

        if (offset >= size || offset + count > size)
        {
            return false;
        }

        const ubyte* data = Data();

        Memory::Copy(out, data + offset, count);

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

    /*! \brief Returns a reference to the byte at \p index in the ByteBuffer.
     *  \deprecated Use \ref Read to read bytes from the ByteBuffer in bulk.
     *  \warning The index must be within the bounds of the ByteBuffer or undefined behavior will occur.
     *  \param index The index of the byte to return.
     *  \return A reference to the byte at the given index. */
    HYP_DEPRECATED HYP_FORCE_INLINE ubyte& operator[](size_t index)
    {
        return *(m_allocation.GetBuffer() + index);
    }

    /*! \brief Returns a the byte at \p index in the ByteBuffer by value.
     *  \deprecated Use \ref Read to read bytes from the ByteBuffer in bulk.
     *  \warning The index must be within the bounds of the ByteBuffer or undefined behavior will occur.
     *  \param index The index of the byte to return.
     *  \return The byte at the given index. */
    HYP_DEPRECATED HYP_FORCE_INLINE ubyte operator[](size_t index) const
    {
        return *(m_allocation.GetBuffer() + index);
    }

    /*! \brief Compares this ByteBuffer with another ByteBuffer for equality.
     *  Two ByteBuffers are considered equal if they have the same size and their contents are identical.
     *  \param other The other ByteBuffer to compare with.
     *  \return True if the ByteBuffers are equal, false otherwise. */
    template <class OtherAllocator>
    HYP_FORCE_INLINE bool operator==(const TByteBuffer<OtherAllocator>& other) const
    {
        if (m_size != other.m_size)
        {
            return false;
        }

        return Memory::Compare(Data(), other.Data(), m_size) == 0;
    }

    /*! \brief Compares this ByteBuffer with another ByteBuffer for inequality.
     *  Two ByteBuffers are considered unequal if they have different sizes or their contents differ.
     *  \param other The other ByteBuffer to compare with.
     *  \return True if the ByteBuffers are not equal, false otherwise. */
    template <class OtherAllocator>
    HYP_FORCE_INLINE bool operator!=(const TByteBuffer<OtherAllocator>& other) const
    {
        if (m_size != other.m_size)
        {
            return true;
        }

        return Memory::Compare(Data(), other.Data(), m_size) != 0;
    }

    /*! \brief Returns a copy of the ByteBuffer.
     *  \return A new ByteBuffer with the same size and contents as this ByteBuffer. */
    HYP_NODISCARD HYP_FORCE_INLINE TByteBuffer Copy() const
    {
        return TByteBuffer(m_size, Data());
    }

    /*! \brief Clears the ByteBuffer, freeing the allocated memory and resetting the size to zero.
     *  After calling this function, the ByteBuffer will be empty and no memory will be allocated. */
    void Clear()
    {
        m_allocation.Free(GetAllocator());
        m_size = 0;
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
    HYP_FORCE_INLINE static AllocatorType* GetAllocator()
    {
        return GetDefaultAllocatorInstance<AllocatorType>();
    }

    Allocation<ubyte, AllocatorType> m_allocation;
    size_t m_size;
};

using ByteBuffer = TByteBuffer<DynamicAllocator>;

template <size_t Size>
using FixedByteBuffer = TByteBuffer<FixedAllocator<Size>>;

} // namespace memory

using memory::ByteBuffer;
using memory::FixedByteBuffer;
using memory::TByteBuffer;

} // namespace Hyperion
