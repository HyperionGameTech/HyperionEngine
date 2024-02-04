#ifndef HYPERION_V2_LIB_BYTE_BUFFER_HPP
#define HYPERION_V2_LIB_BYTE_BUFFER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/Span.hpp>
#include <core/lib/CMemory.hpp>
#include <HashCode.hpp>
#include <Constants.hpp>

#include <Types.hpp>

namespace hyperion {

using ByteArray = Array<UByte>;

using ByteView = Span<UByte>;
using ConstByteView = Span<const UByte>;

/*! \brief An immutable array of bytes, which for large buffers, shares the memory with any copied objects */
class ByteBuffer
{
    using InternalArray = Array<UByte, 1024u>;

public:
    ByteBuffer() = default;

    ByteBuffer(SizeType count)
    {
        SetSize(count);
    }

    ByteBuffer(SizeType count, const void *data)
    {
        SetData(count, data);
    }

    explicit ByteBuffer(const ByteView &view)
    {
        SetData(view.size, view.ptr);
    }

    explicit ByteBuffer(const ConstByteView &view)
    {
        SetData(view.size, view.ptr);
    }

    ByteBuffer(const ByteBuffer &other)
        : m_internal(other.m_internal)
    {
    }

    ByteBuffer &operator=(const ByteBuffer &other)
    {
        m_internal = other.m_internal;

        return *this;
    }

    ByteBuffer(ByteBuffer &&other) noexcept
        : m_internal(std::move(other.m_internal))
    {
    }

    ByteBuffer &operator=(ByteBuffer &&other) noexcept
    {
        m_internal = std::move(other.m_internal);

        return *this;
    }

    ~ByteBuffer() = default;

    explicit operator bool() const
        { return true; }

    void Write(SizeType count, SizeType offset, const void *data)
    {
        if (count == 0) {
            return;
        }

        AssertThrow(offset + count <= Size());

        Memory::MemCpy(m_internal.Data() + offset, data, count);
    }

    /**
     * \brief Updates the ByteBuffer's data with the given data.
     */
    void SetData(SizeType count, const void *data)
    {
        m_internal.Resize(count);

        if (count == 0) {
            return;
        }

        Memory::MemCpy(m_internal.Data(), data, count);
    }

    /**
     * \brief Returns a reference to the ByteBuffer's internal array
     */
    InternalArray &GetInternalArray()
    {
        return m_internal;
    }

    /**
     * \brief Returns a const reference to the ByteBuffer's internal array. The reference is only valid as long as the ByteBuffer is not modified.
     */
    const InternalArray &GetInternalArray() const
        { return const_cast<ByteBuffer *>(this)->GetInternalArray(); }

    /**
     * \brief Returns a copy of the ByteBuffer's data.
     */
    ByteArray ToByteArray() const
    {
        const auto size = GetInternalArray().Size();
        const auto *data = GetInternalArray().Data();

        ByteArray byte_array;
        byte_array.Resize(size);
        Memory::MemCpy(byte_array.Data(), data, size);

        return byte_array;
    }

    /**
     * \brief Returns a ByteView of the ByteBuffer's data. The ByteView is only valid as long as the ByteBuffer is not modified.
     */
    ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (size > Size()) {
            size = Size();
        }

        return ByteView(Data() + offset, size);
    }

    /**
     * \brief Returns a ConstByteView of the ByteBuffer's data. The ConstByteView is only valid as long as the ByteBuffer is not modified.
     */
    ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (size > Size()) {
            size = Size();
        }

        return ConstByteView(Data() + offset, size);
    }

    /*! \brief Be aware that modifying the ByteBuffer's data could have unintentional consequences if
        it is sharing memory with other ByteBuffers. */
    UByte *Data()
        { return GetInternalArray().Data(); }

    const UByte *Data() const
        { return GetInternalArray().Data(); }

    /**
     * \brief Reads a value from the ByteBuffer at the given offset.
     */
    bool Read(SizeType offset, SizeType count, UByte *out_values) const
    {
        AssertThrow(out_values != nullptr);

        const SizeType size = Size();

        if (offset >= size || offset + count > size) {
            return false;
        }

        const UByte *data = Data();

        for (SizeType index = offset; index < offset + count; index++) {
            out_values[index - offset] = data[index];
        }

        return true;
    }

    /**
     * \brief Reads a value from the ByteBuffer at the given offset.
     */
    template <class T>
    bool Read(SizeType offset, T *out) const
    {
        static_assert(IsPODType<T>, "Must be POD type");

        AssertThrow(out != nullptr);

        constexpr SizeType count = sizeof(T);
        const SizeType size = Size();

        if (offset >= size || offset + count > size) {
            return false;
        }

        const UByte *data = Data();

        alignas(T) UByte bytes[sizeof(T)];

        for (SizeType index = offset; index < offset + count; index++) {
            bytes[index - offset] = data[index];
        }

        Memory::MemCpy(out, bytes, sizeof(T));

        return true;

    }

    SizeType Size() const
        { return GetInternalArray().Size(); }
    
    void SetSize(SizeType count)
    {
        if (count == Size()) {
            return;
        }

        m_internal.Resize(count);
    }

    /**
     * \brief Returns true if the ByteBuffer has any elements.
     */
    bool Any() const
        { return Size() != 0; }

    /**
     * \brief Returns true if the ByteBuffer has no elements.
     */
    bool Empty() const
        { return Size() == 0; }

    const UByte &operator[](SizeType index) const
        { return GetInternalArray()[index]; }

    bool operator==(const ByteBuffer &other) const
        { return m_internal == other.m_internal; }

    bool operator!=(const ByteBuffer &other) const
        { return m_internal != other.m_internal; }

    /**
     * \brief Returns a copy of the ByteBuffer, which is guaranteed to not share memory with the original.
     */
    ByteBuffer Copy() const
    {
        return ByteBuffer(Size(), Data());
    }

    HashCode GetHashCode() const
        { return GetInternalArray().GetHashCode(); }

private:
    InternalArray   m_internal;
};

} // namespace hyperion

#endif