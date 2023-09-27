#ifndef HYPERION_V2_LIB_BYTE_BUFFER_HPP
#define HYPERION_V2_LIB_BYTE_BUFFER_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Variant.hpp>
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
    ByteBuffer()
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
    }

    ByteBuffer(SizeType count)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
        SetSize(count);
    }

    ByteBuffer(SizeType count, const void *data)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
        SetData(count, data);
    }

    explicit ByteBuffer(const ByteView &view)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
        SetData(view.size, view.ptr);
    }

    explicit ByteBuffer(const ConstByteView &view)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
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

    /**
     * \brief Updates the ByteBuffer's data. If the size would go beyond the number of inline elements that can be stored, the ByteBuffer will switch to a reference counted internal byte array.
     */
    void SetData(SizeType count, const void *data)
    {
        if (count > InternalArray::num_inline_elements) {
            m_internal.Set(RefCountedPtr<InternalArray>(new InternalArray()));

            if (count != 0) {
                auto &byte_array = m_internal.Get<RefCountedPtr<InternalArray>>();

                byte_array->Resize(count);

                Memory::MemCpy(byte_array->Data(), data, count);
            }
        } else {
            m_internal.Set(InternalArray { });

            if (count != 0) {
                auto &byte_array = m_internal.Get<InternalArray>();

                byte_array.Resize(count);

                Memory::MemCpy(byte_array.Data(), data, count);
            }
        }
    }

    /**
     * \brief Returns a reference to the ByteBuffer's internal array. The reference is only valid as long as the ByteBuffer is not modified.
     */
    InternalArray &GetInternalArray()
    {
        if (m_internal.Is<RefCountedPtr<InternalArray>>()) {
            return *m_internal.Get<RefCountedPtr<InternalArray>>();
        } else {
            return m_internal.Get<InternalArray>();
        }
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
        { return m_internal.GetPointer() == other.m_internal.GetPointer(); }

    bool operator!=(const ByteBuffer &other) const
        { return m_internal.GetPointer() != other.m_internal.GetPointer(); }

    /**
     * \brief Returns true if the ByteBuffer is sharing memory with other ByteBuffers.
     */
    bool IsShared() const
    {
        const auto *ptr = m_internal.TryGet<RefCountedPtr<InternalArray>>();

        if (!ptr) {
            return false;
        }

        return ptr->GetRefCountData()->UseCount() > 1;
    }

    /**
     * \brief Returns a copy of the ByteBuffer. If the ByteBuffer is sharing memory with other ByteBuffers, the copy will be unique. Otherwise, the copy may share memory with the original.
     */
    ByteBuffer Lock() const
    {
        if (IsShared()) {
            return Copy();
        }

        return *this;
    }

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
    Variant<InternalArray, RefCountedPtr<InternalArray>> m_internal;

    void SetSize(SizeType count)
    {
        if (count > InternalArray::num_inline_elements) {
            m_internal.Set(RefCountedPtr<InternalArray>(new InternalArray()));

            auto &byte_array = m_internal.Get<RefCountedPtr<InternalArray>>();

            byte_array->Resize(count);
        } else {
            m_internal.Set(InternalArray { });

            auto &byte_array = m_internal.Get<InternalArray>();

            byte_array.Resize(count);
        }
    }
};

} // namespace hyperion

#endif