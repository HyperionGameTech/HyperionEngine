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

    InternalArray &GetInternalArray()
    {
        if (m_internal.Is<RefCountedPtr<InternalArray>>()) {
            return *m_internal.Get<RefCountedPtr<InternalArray>>();
        } else {
            return m_internal.Get<InternalArray>();
        }
    }

    const InternalArray &GetInternalArray() const
        { return const_cast<ByteBuffer *>(this)->GetInternalArray(); }

    ByteArray ToByteArray() const
    {
        const auto size = GetInternalArray().Size();
        const auto *data = GetInternalArray().Data();

        ByteArray byte_array;
        byte_array.Resize(size);
        Memory::MemCpy(byte_array.Data(), data, size);

        return byte_array;
    }

    /*! \brief Be aware that modifying the ByteBuffer's data could have unintentional consequences if
        it is sharing memory with other ByteBuffers. */
    UByte *Data()
        { return GetInternalArray().Data(); }

    const UByte *Data() const
        { return GetInternalArray().Data(); }

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

    bool Any() const
        { return Size() != 0; }

    bool Empty() const
        { return Size() == 0; }

    const UByte &operator[](SizeType index) const
        { return GetInternalArray()[index]; }

    bool operator==(const ByteBuffer &other) const
        { return GetInternalArray() == other.GetInternalArray(); }

    bool operator!=(const ByteBuffer &other) const
        { return GetInternalArray() != other.GetInternalArray(); }

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