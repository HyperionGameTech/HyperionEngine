#ifndef HYPERION_V2_LIB_DYN_ARRAY_H
#define HYPERION_V2_LIB_DYN_ARRAY_H

#include "ContainerBase.hpp"
#include "Pair.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>
#include <core/lib/CMemory.hpp>
#include <system/Debug.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>

#include <algorithm>
#include <utility>
#include <cmath>

namespace hyperion {

/*! Vector class with smart front removal and inline storage so small lists
    do not require any heap allocation (and are faster than using a st::vector).
    Otherwise, average speed is about the same as std::vector in most cases.
    NOTE: will use a bit more memory than vector, partially because of inline storage in the class, partially due to
    zero deallocations/shifting on PopFront().
*/

template <class T>
class DynArray : public ContainerBase<DynArray<T>, UInt> {
public:
    using Base = DynArray<T>;

    using SizeType = UInt64;
    using ValueType = T;

    static constexpr SizeType num_inline_bytes = 512u;
    static constexpr SizeType num_inline_elements = MathUtil::Max(num_inline_bytes / sizeof(T), 1u);

protected:
    // on PushFront() we can pad the start with this number,
    // so when multiple successive calls to PushFront() happen,
    // we're not realloc'ing everything each time
    static constexpr SizeType push_front_padding = 4;

public:
    using Iterator      = T *;
    using ConstIterator = const T *;
    using InsertResult  = Pair<Iterator, bool>; // iterator, was inserted

    DynArray();

    template <SizeType Sz>
    DynArray(T const (&items)[Sz])
        : DynArray()
    {
        Reserve(Sz);

        for (SizeType i = 0; i < Sz; i++) {
            PushBack(items[i]);
        }
    }

    DynArray(const std::initializer_list<T> &items)
        : DynArray()
    {
        Reserve(items.size());

        for (auto it = items.begin(); it != items.end(); ++it) {
            PushBack(*it);
        }
    }

    DynArray(const DynArray &other);
    DynArray(DynArray &&other) noexcept;
    ~DynArray();

    DynArray &operator=(const DynArray &other);
    DynArray &operator=(DynArray &&other) noexcept;

    /*[[nodiscard]] SizeType Size() const                             { return m_size - m_start_offset; }

    [[nodiscard]] ValueType *Data()                                 { return &m_values[m_start_offset]; }
    [[nodiscard]] const ValueType *Data() const                     { return &m_values[m_start_offset]; }

    [[nodiscard]] ValueType &Front()                                { return m_values[m_start_offset]; }
    [[nodiscard]] const ValueType &Front() const                    { return m_values[m_start_offset]; }

    [[nodiscard]] ValueType &Back()                                 { return m_values[m_size - 1]; }
    [[nodiscard]] const ValueType &Back() const                     { return m_values[m_size - 1]; }

    [[nodiscard]] bool Empty() const                                { return Size() == 0; }
    [[nodiscard]] bool Any() const                                  { return Size() != 0; }

    ValueType &operator[](SizeType index)                           { return m_values[m_start_offset + index]; }
    [[nodiscard]] const ValueType &operator[](SizeType index) const { return m_values[m_start_offset + index]; }
    */

    
    [[nodiscard]] SizeType Size() const                             { return m_size - m_start_offset; }

    [[nodiscard]] ValueType *Data()                                 { return &reinterpret_cast<T *>(m_buffer)[m_start_offset]; }
    [[nodiscard]] const ValueType *Data() const                     { return &reinterpret_cast<const T *>(m_buffer)[m_start_offset]; }

    [[nodiscard]] ValueType &Front()                                { return m_buffer[m_start_offset].Get(); }
    [[nodiscard]] const ValueType &Front() const                    { return m_buffer[m_start_offset].Get(); }

    [[nodiscard]] ValueType &Back()                                 { return m_buffer[m_size - 1].Get(); }
    [[nodiscard]] const ValueType &Back() const                     { return m_buffer[m_size - 1].Get(); }

    [[nodiscard]] bool Empty() const                                { return Size() == 0; }
    [[nodiscard]] bool Any() const                                  { return Size() != 0; }

    ValueType &operator[](SizeType index)                           { return m_buffer[m_start_offset + index].Get(); }
    [[nodiscard]] const ValueType &operator[](SizeType index) const { return m_buffer[m_start_offset + index].Get(); }

    void Reserve(SizeType capacity);
    void Resize(SizeType new_size);
    void Refit();
    void PushBack(const ValueType &value);
    void PushBack(ValueType &&value);

    /*! Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time.
        */
    void PushFront(const ValueType &value);

    /*! Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time. */
    void PushFront(ValueType &&value);

    /*! Shift the array to the left by {count} times */
    void Shift(SizeType count);

    void Concat(const DynArray &other);
    void Concat(DynArray &&other);
    //! \brief Erase an element by iterator.
    Iterator Erase(ConstIterator iter);
    //! \brief Erase an element by value. A Find() is performed, and if the result is not equal to End(),
    //  the element is removed.
    Iterator Erase(const T &value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType &value);
    Iterator Insert(ConstIterator where, ValueType &&value);
    ValueType PopFront();
    ValueType PopBack();
    void Clear();

    HYP_DEF_STL_BEGIN_END(
        ToValueTypePtr(m_buffer[m_start_offset]),
        ToValueTypePtr(m_buffer[m_size])
    )

protected:
    void ResetOffsets();
    void SetCapacity(SizeType capacity, SizeType copy_offset = 0);

    Iterator RealBegin() const { return ToValueTypePtr(m_buffer[0]); }
    Iterator RealEnd() const { return ToValueTypePtr(m_buffer[m_size]); }

    static SizeType GetCapacity(SizeType size)
    {
        return 1ull << static_cast<SizeType>(std::ceil(std::log(size) / std::log(2.0)));
    }

    SizeType m_size;
    SizeType m_capacity;
    
    struct alignas(T) Storage {
        alignas(T) std::byte data_buffer[sizeof(T)];

        ValueType &Get()
        {
            return *reinterpret_cast<T *>(&data_buffer);
        }

        [[nodiscard]] const ValueType &Get() const
        {
            return *reinterpret_cast<const T *>(&data_buffer);
        }
    };

    static_assert(sizeof(Storage) == sizeof(Storage::data_buffer), "Storage struct should not be padded");
    // static_assert(alignof(Storage) == alignof(Storage::data_buffer), "Storage struct should not be padded");

    HYP_FORCE_INLINE
    static ValueType &ToValueType(Storage &storage)
    {
        return *reinterpret_cast<ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static const ValueType &ToValueType(const Storage &storage)
    {
        return *reinterpret_cast<const ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static ValueType *ToValueTypePtr(Storage &storage)
    {
        return reinterpret_cast<ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static const ValueType *ToValueTypePtr(const Storage &storage)
    {
        return reinterpret_cast<const ValueType *>(&storage);
    }

    // dynamic memory
    union {
        Storage *m_buffer;
        ValueType *m_values;
    };

    Storage m_inline_buffer[num_inline_elements];
    bool m_is_dynamic;

    typename DynArray::Base::KeyType m_start_offset;
};

template <class T>
DynArray<T>::DynArray()
    : m_size(0),
      m_capacity(num_inline_elements),
      m_buffer(&m_inline_buffer[0]),
      m_is_dynamic(false),
      m_start_offset(0)
{
#if HYP_DEBUG_MODE
    Memory::Garble(&m_inline_buffer[0], num_inline_elements * sizeof(Storage));
#endif
}

template <class T>
DynArray<T>::DynArray(const DynArray &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(nullptr),
      m_is_dynamic(other.m_is_dynamic),
      m_start_offset(other.m_start_offset)
{
    if (m_is_dynamic) {
        m_buffer = static_cast<Storage *>(std::malloc(sizeof(Storage) * other.m_capacity));
    } else {
        m_buffer = &m_inline_buffer[0];
    }

    // copy all members
    for (SizeType i = m_start_offset; i < m_size; i++) {
        Memory::Construct<T>(&m_buffer[i].data_buffer, other.m_buffer[i].Get());
    }
}

template <class T>
DynArray<T>::DynArray(DynArray &&other) noexcept
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(nullptr),
      m_is_dynamic(other.m_is_dynamic),
      m_start_offset(other.m_start_offset)
{
    if (m_is_dynamic) {
        m_buffer = other.m_buffer;
    } else {
        m_buffer = &m_inline_buffer[0];

        // move all members
        for (SizeType i = m_start_offset; i < m_size; i++) {
            Memory::Construct<T>(&m_buffer[i].data_buffer, std::move(other.m_buffer[i].Get()));
        }
    }

    other.m_size        = 0;
    other.m_capacity    = num_inline_elements;
    other.m_buffer      = &other.m_inline_buffer[0];
    other.m_is_dynamic  = false;
    other.m_start_offset = 0;
}

template <class T>
DynArray<T>::~DynArray()
{
    for (Int64 i = m_size - 1; i >= m_start_offset; --i) {
        Memory::Destruct(m_buffer[i].Get());
    }
    
    // only nullptr if it has been move()'d in which case size would be 0
    //delete[] m_buffer;
    if (m_is_dynamic) {
        std::free(m_buffer);
    }
}

template <class T>
auto DynArray<T>::operator=(const DynArray &other) -> DynArray&
{
    if (this == std::addressof(other)) {
        return *this;
    }

    if (m_capacity < other.m_capacity) {
        for (Int64 i = m_size - 1; i >= m_start_offset; --i) {
            Memory::Destruct(m_buffer[i].Get());
        }

        if (m_is_dynamic) {
            std::free(m_buffer);
        }

        if (other.m_is_dynamic) {
            m_is_dynamic = true;
            m_buffer     = static_cast<Storage *>(std::malloc(sizeof(Storage) * other.m_capacity));
            m_capacity   = other.m_capacity;
        } else {
            m_buffer     = &m_inline_buffer[0];
            m_is_dynamic = false;
            m_capacity   = num_inline_elements;
        }

        m_size         = other.m_size;
        m_start_offset = other.m_start_offset;

        // copy all objects
        for (SizeType i = m_start_offset; i < m_size; i++) {
            Memory::Construct<T>(&m_buffer[i].data_buffer, other.m_buffer[i].Get());
        }
    } else {
        // have to destroy excess items
        if (other.Size() < Size()) {
            for (Int64 i = Size(); i > other.Size(); --i) {
                Memory::Destruct(m_buffer[m_start_offset + i - 1].Get());
            }
        }

        // capacity already fits, no need to reallocate memory.
        for (SizeType i = m_start_offset, j = other.m_start_offset; i < m_size; ++i, ++j) {
            m_buffer[i].Get() = other.m_buffer[j].Get();
        }

        // have to construct new items.
        for (SizeType i = m_size, j = other.m_start_offset + Size(); j < other.m_size; ++i, ++j) {
            Memory::Construct<T>(&m_buffer[i].data_buffer, other.m_buffer[j].Get());
        }

        m_size = other.Size() + m_start_offset;
        // keep start index, buffer, capacity
    }

    return *this;
}

template <class T>
auto DynArray<T>::operator=(DynArray &&other) noexcept -> DynArray&
{
    for (Int64 i = m_size - 1; i >= m_start_offset; --i) {
        Memory::Destruct(m_buffer[i].Get());
    }

    if (m_is_dynamic) {
        std::free(m_buffer);
    }


    if (other.m_is_dynamic) {
        m_size         = other.m_size;
        m_capacity     = other.m_capacity;
        m_buffer       = other.m_buffer;
        m_start_offset = other.m_start_offset;
        m_is_dynamic   = true;
    } else {
        m_buffer       = &m_inline_buffer[0];
        m_capacity     = num_inline_elements;
        m_is_dynamic   = false;

        // move items individually
        for (SizeType i = 0; i < other.Size(); i++) {
            Memory::Construct<T>(&m_buffer[i].data_buffer, std::move(other.m_buffer[other.m_start_offset + i].Get()));
        }

        m_size         = other.Size();
        m_start_offset = 0;

        // manually call destructors
        for (Int64 i = other.m_size - 1; i >= other.m_start_offset; --i) {
            Memory::Destruct(other.m_buffer[i].Get());
        }
    }

    other.m_size = 0;
    other.m_capacity = num_inline_elements;
    other.m_is_dynamic = false;
    other.m_buffer = &other.m_inline_buffer[0];
    other.m_start_offset = 0;

    return *this;
}

template <class T>
void DynArray<T>::ResetOffsets()
{
    if (m_start_offset == 0) {
        return;
    }

    // shift all items to left
    for (SizeType index = m_start_offset; index < m_size; index++) {
        const auto move_index = index - m_start_offset;

        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&m_buffer[move_index].data_buffer, std::move(m_buffer[index].Get()));
        } else {
            Memory::Construct<T>(&m_buffer[move_index].data_buffer, m_buffer[index].Get());
        }

        // manual destructor call
        Memory::Destruct(m_buffer[index].Get());
    }

    m_size -= m_start_offset;
    m_start_offset = 0;
}

template <class T>
void DynArray<T>::SetCapacity(SizeType capacity, SizeType copy_offset)
{
    if (capacity > num_inline_elements) {
        // delete and copy all over again
        auto *new_buffer = static_cast<Storage *>(std::malloc(sizeof(Storage) * capacity));

        // AssertThrow(Size() <= m_capacity);
        
        for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
            if constexpr (std::is_move_constructible_v<T>) {
                Memory::Construct<T>(&new_buffer[i].data_buffer, std::move(m_buffer[j].Get()));
            } else {
                Memory::Construct<T>(&new_buffer[i].data_buffer, m_buffer[j].Get());
            }
        }

        // manually call destructors of old buffer
        for (Int64 i = m_size - 1; i >= m_start_offset; --i) {
            Memory::Destruct(m_buffer[i].Get());
        }

        if (m_is_dynamic) {
            // delete old buffer memory
            std::free(m_buffer);
        }

        // set internal buffer to the new one
        m_capacity = capacity;
        m_size -= static_cast<Int64>(m_start_offset) - static_cast<Int64>(copy_offset);
        m_buffer = new_buffer;
        m_is_dynamic = true;
        m_start_offset = copy_offset;
    } else {
        if (m_is_dynamic) { // switch from dynamic to non-dynamic
            for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
                m_inline_buffer[i].Get() = std::move(m_buffer[j].Get());
            }

            // call destructors on old buffer
            for (Int64 i = m_size - 1; i >= m_start_offset; --i) {
                Memory::Destruct(m_buffer[i].Get());
            }

            std::free(m_buffer);

            m_is_dynamic = false;
            m_buffer = &m_inline_buffer[0];
            m_capacity = num_inline_elements;
        } else if (m_start_offset != copy_offset) {
            if (m_start_offset > copy_offset) {
                const SizeType diff = m_start_offset - copy_offset;

                // shift left
                for (SizeType index = m_start_offset; index < m_size; ++index) {
                    const auto move_index = index - diff;

                    if constexpr (std::is_move_constructible_v<T>) {
                        Memory::Construct<T>(&m_buffer[move_index].data_buffer, std::move(m_buffer[index].Get()));
                    } else {
                        Memory::Construct<T>(&m_buffer[move_index].data_buffer, m_buffer[index].Get());
                    }

                    // manual destructor call
                    Memory::Destruct(m_buffer[index].Get());
                }
            } else {
                // shift right
                const SizeType diff = copy_offset - m_start_offset;

                for (Int64 index = m_size - 1; index >= m_start_offset; --index) {
                    const auto move_index = index + diff;

                    if constexpr (std::is_move_constructible_v<T>) {
                        Memory::Construct<T>(&m_buffer[move_index].data_buffer, std::move(m_buffer[index].Get()));
                    } else {
                        Memory::Construct<T>(&m_buffer[move_index].data_buffer, m_buffer[index].Get());
                    }

                    // manual destructor call
                    Memory::Destruct(m_buffer[index].Get());
                }
            }
        }

        m_size -= static_cast<Int64>(m_start_offset) - static_cast<Int64>(copy_offset);
        m_start_offset = copy_offset;

        // not currently dynamic; no need to reduce capacity of inline buffer
    }
}

template <class T>
void DynArray<T>::Reserve(SizeType capacity)
{
    if (m_capacity >= capacity) {
        return;
    }

    SetCapacity(capacity);
}

template <class T>
void DynArray<T>::Resize(SizeType new_size)
{
    if (new_size == Size()) {
        return;
    }

    if (new_size > Size()) {
        const SizeType diff = new_size - Size();

        if (m_size + diff >= m_capacity) {
            if (m_capacity >= Size() + diff) {
                ResetOffsets();
            } else {
                SetCapacity(GetCapacity(Size() + diff));
            }
        }

        while (Size() < new_size) {
            // construct item at index
            Memory::Construct<T>(&m_buffer[m_size++].data_buffer);
        }
    } else {
        while (new_size < Size()) {
            PopBack();
        }
    }
}

template <class T>
void DynArray<T>::Refit()
{
    if (m_capacity == Size()) {
        return;
    }

    SetCapacity(Size());
}

template <class T>
void DynArray<T>::PushBack(const ValueType &value)
{
    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    // set item at index
    Memory::Construct<T>(&m_buffer[m_size++].data_buffer, value);
}

template <class T>
void DynArray<T>::PushBack(ValueType &&value)
{
    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    AssertThrow(m_capacity >= m_size + 1);

    // set item at index
    Memory::Construct<T>(&m_buffer[m_size++].data_buffer, std::forward<ValueType>(value));
}

template <class T>
void DynArray<T>::PushFront(const ValueType &value)
{
    if (m_start_offset == 0) {
        // have to push everything else over by 1
        if (m_size + push_front_padding >= m_capacity) {
            SetCapacity(
                GetCapacity(Size() + push_front_padding),
                push_front_padding // copy_offset is 1 so we have a space for 1 at the start
            );
        } else {
            // shift over without realloc
            for (Int64 index = Size() - 1; index >= 0; --index) {
                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&m_buffer[move_index].data_buffer, std::move(m_buffer[index].Get()));
                } else {
                    Memory::Construct<T>(&m_buffer[move_index].data_buffer, m_buffer[index].Get());
                }

                // manual destructor call
                Memory::Destruct(m_buffer[index].Get());
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    Memory::Construct<T>(&m_buffer[m_start_offset].data_buffer, value);
}

template <class T>
void DynArray<T>::PushFront(ValueType &&value)
{
    if (m_start_offset == 0) {
        // have to push everything else over by 1
        if (m_size + push_front_padding >= m_capacity) {
            SetCapacity(
                GetCapacity(Size() + push_front_padding),
                push_front_padding // copy_offset is 1 so we have a space for 1 at the start
            );
        } else {
            // shift over without realloc
            for (Int64 index = Size() - 1; index >= 0; --index) {
                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&m_buffer[move_index].data_buffer, std::move(m_buffer[index].Get()));
                } else {
                    Memory::Construct<T>(&m_buffer[move_index].data_buffer, m_buffer[index].Get());
                }

                // manual destructor call
                Memory::Destruct(m_buffer[index].Get());
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    Memory::Construct<T>(&m_buffer[m_start_offset].data_buffer, std::move(value));
}

template <class T>
void DynArray<T>::Concat(const DynArray &other)
{
    if (m_size + other.Size() >= m_capacity) {
        if (m_capacity >= Size() + other.Size()) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + other.Size()));
        }
    }

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        Memory::Construct<T>(&m_buffer[m_size++].data_buffer, other[i]);
    }
}

template <class T>
void DynArray<T>::Concat(DynArray &&other)
{
    if (m_size + other.Size() >= m_capacity) {
        if (m_capacity >= Size() + other.Size()) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + other.Size()));
        }
    }

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        Memory::Construct<T>(&m_buffer[m_size++].data_buffer, std::move(other[i]));
    }

    other.Clear();
}

template <class T>
void DynArray<T>::Shift(SizeType count)
{
    SizeType new_size = 0;

    for (SizeType index = m_start_offset; index < m_size; index++, new_size++) {
        if (index + count >= m_size) {
            break;
        }

        if constexpr (std::is_move_assignable_v<T>) {
            m_buffer[index].Get() = std::move(m_buffer[index + count].Get());
        } else if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(m_buffer[index].Get());
            Memory::Construct<T>(&m_buffer[index].data_buffer, std::move(m_buffer[index + count].Get()));
        } else {
            m_buffer[index].Get() = m_buffer[index + count].Get();
        }

        // manual destructor call
        Memory::Destruct(m_buffer[index + count].Get());
    }

    m_size = new_size;
}

template <class T>
auto DynArray<T>::Erase(ConstIterator iter) -> Iterator
{
    const Int64 dist = iter - Begin();

    for (Int64 index = dist; index < Size() - 1; ++index) {
        if constexpr (std::is_move_assignable_v<T>) {
            m_buffer[m_start_offset + index].Get() = std::move(m_buffer[m_start_offset + index + 1].Get());
        } else if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(m_buffer[m_start_offset + index].Get());
            Memory::Construct<T>(&m_buffer[m_start_offset + index].data_buffer, std::move(m_buffer[m_start_offset + index + 1].Get()));
        } else {
            m_buffer[m_start_offset + index].Get() = m_buffer[m_start_offset + index + 1].Get();
        }
    }

    Memory::Destruct(m_buffer[m_size - 1].Get());
    --m_size;
    
    return Begin() + dist;
}

template <class T>
auto DynArray<T>::Erase(const T &value) -> Iterator
{
    ConstIterator iter = Find(value);

    if (iter != End()) {
        return Erase(iter);
    }

    return End();
}

template <class T>
auto DynArray<T>::EraseAt(typename DynArray::Base::KeyType index) -> Iterator
{
    return Erase(Begin() + index);
}

template <class T>
auto DynArray<T>::Insert(ConstIterator where, const ValueType &value) -> Iterator
{
    const Int64 dist = where - Begin();

    if (where == End()) {
        PushBack(value);

        return &m_values[m_size - 1];
    } else if (where == Begin() && dist <= m_start_offset) {
        PushFront(value);

        return Begin();
    }

    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    AssertThrow(m_capacity >= m_size + 1);

    Int64 index;

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, std::move(m_buffer[index + m_start_offset - 1].Get()));
        } else {
            Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, m_buffer[index + m_start_offset - 1].Get());
        }

        Memory::Destruct(m_buffer[index + m_start_offset - 1].Get());
    }

    Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, value);

    ++m_size;

    return Begin() + index;
}


template <class T>
auto DynArray<T>::Insert(ConstIterator where, ValueType &&value) -> Iterator
{
    const Int64 dist = where - Begin();

    if (where == End()) {
        PushBack(std::move(value));
        
        return &m_values[m_size - 1];
    } else if (where == Begin() && dist <= m_start_offset) {
        PushFront(std::move(value));

        return Begin();
    }

    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    AssertThrow(m_capacity >= m_size + 1);

    Int64 index;

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, std::move(m_buffer[index + m_start_offset - 1].Get()));
        } else {
            Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, m_buffer[index + m_start_offset - 1].Get());
        }

        Memory::Destruct(m_buffer[index + m_start_offset - 1].Get());
    }

    Memory::Construct<T>(&m_buffer[index + m_start_offset].data_buffer, std::move(value));

    ++m_size;

    return Begin() + index;
}

template <class T>
auto DynArray<T>::PopFront() -> ValueType
{
    AssertThrow(Size() != 0);

    auto value = std::move(m_buffer[m_start_offset].Get());

    Memory::Destruct(m_buffer[m_start_offset].Get());

    ++m_start_offset;

    return value;
}

template <class T>
auto DynArray<T>::PopBack() -> ValueType
{
    AssertThrow(m_size != 0);

    auto value = std::move(m_buffer[m_size - 1].Get());

    Memory::Destruct(m_buffer[m_size - 1].Get());

    --m_size;

    return value;
}

template <class T>
void DynArray<T>::Clear()
{
    while (m_size - m_start_offset) {
        // manual destructor call
        Memory::Destruct(m_buffer[m_size - 1].Get());
        --m_size;
    }

    m_size = 0;
    m_start_offset = 0;

    Refit();
}

// deduction guide
// template <typename Tp, typename ...Args>
// DynArray(Tp, Args...) -> DynArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace hyperion

#endif