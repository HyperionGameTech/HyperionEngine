/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/ContainerBase.hpp>

#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Defines.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>
#include <Core/Utilities/Traits.hpp>
#include <Core/HashCode.hpp>

#include <algorithm>
#include <cstring>
#include <utility>
#include <cmath>

namespace Hyperion {

namespace containers {

template <class TElemType, class TAllocator = DynamicAllocator>
class SlimArray : public ContainerBase<SlimArray<TElemType, TAllocator>, size_t>
{
public:
    static constexpr bool isContiguous = true;

    using ValueType = TElemType;
    using Base = ContainerBase<SlimArray<TElemType, TAllocator>, size_t>;
    using KeyType = typename Base::KeyType;

    // Allow other SlimArray types to access private members
    template <class, class>
    friend class SlimArray;

    using Iterator = TElemType*;
    using ConstIterator = const TElemType*;
    using InsertResult = Pair<Iterator, bool>;

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray()
        : data(nullptr),
          size(0),
          capacity(0)
    {
    }

    SlimArray(const SlimArray& other);
    SlimArray(SlimArray&& other) noexcept;

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    explicit SlimArray(uint32 size)
        : SlimArray()
    {
        Resize(size);
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(Span<TElemType> span)
        : SlimArray(span.Data(), span.Size())
    {
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(Span<const TElemType> span)
        : SlimArray(span.Data(), span.Size())
    {
    }

    template <uint32 Sz, bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(TElemType const (&items)[Sz])
        : SlimArray()
    {
        ResizeUninitialized(Sz);

        for (uint32 i = 0; i < Sz; ++i)
        {
            Memory::Construct<TElemType>(&data[i], items[i]);
        }
    }

    template <uint32 Sz, bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(TElemType (&&items)[Sz])
        : SlimArray()
    {
        ResizeUninitialized(Sz);

        for (uint32 i = 0; i < Sz; ++i)
        {
            Memory::Construct<TElemType>(&data[i], std::move(items[i]));
        }
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(TElemType* ptr, uint32 size)
        : SlimArray()
    {
        ResizeUninitialized(size);

        for (uint32 i = 0; i < size; ++i)
        {
            Memory::Construct<TElemType>(&data[i], ptr[i]);
        }
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(Iterator first, Iterator last)
        : SlimArray()
    {
        const uint32 dist = uint32(last - first);
        ResizeUninitialized(dist);

        for (uint32 i = 0; i < dist; ++i)
        {
            Memory::Construct<TElemType>(&data[i], first[i]);
        }
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(ConstIterator first, ConstIterator last)
        : SlimArray()
    {
        const uint32 dist = uint32(last - first);
        ResizeUninitialized(dist);

        for (uint32 i = 0; i < dist; ++i)
        {
            Memory::Construct<TElemType>(&data[i], first[i]);
        }
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(const TElemType* ptr, uint32 size)
        : SlimArray(ptr, ptr + size)
    {
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<TAllocator>, typename = std::enable_if_t<ConditionalEnable>>
    SlimArray(std::initializer_list<TElemType> initializerList)
        : SlimArray(initializerList.begin(), initializerList.end())
    {
    }

    template <class TOtherAllocator, typename = std::enable_if_t<!std::is_same_v<TOtherAllocator, TAllocator> && HasDefaultAllocatorInstance<TAllocator>>>
    explicit SlimArray(const SlimArray<TElemType, TOtherAllocator>& other)
        : SlimArray()
    {
        size = other.Size();

        if (size > 0)
        {
            Allocate(size);
            InitFromRangeCopy(other.Begin(), other.End());
        }
    }

    template <class TOtherAllocator, typename = std::enable_if_t<!std::is_same_v<TOtherAllocator, TAllocator>>>
    SlimArray(SlimArray<TElemType, TOtherAllocator>&& other) noexcept = delete;

    ~SlimArray();

    SlimArray& operator=(const SlimArray& other);
    SlimArray& operator=(SlimArray&& other) noexcept;

    template <class TOtherAllocator, typename = std::enable_if_t<!std::is_same_v<TOtherAllocator, TAllocator>>>
    SlimArray& operator=(const SlimArray<TElemType, TOtherAllocator>& other);

    template <class TOtherAllocator, typename = std::enable_if_t<!std::is_same_v<TOtherAllocator, TAllocator>>>
    SlimArray& operator=(SlimArray<TElemType, TOtherAllocator>&& other) noexcept = delete;

    HYP_FORCE_INLINE uint32 Size() const
    {
        return size;
    }

    HYP_FORCE_INLINE uint32 ByteSize() const
    {
        return size * uint32(sizeof(TElemType));
    }

    HYP_FORCE_INLINE ValueType* Data()
    {
        return data;
    }

    HYP_FORCE_INLINE const ValueType* Data() const
    {
        return data;
    }

    HYP_FORCE_INLINE ValueType& Front()
    {
        return data[0];
    }

    HYP_FORCE_INLINE const ValueType& Front() const
    {
        return data[0];
    }

    HYP_FORCE_INLINE ValueType& Back()
    {
        return data[size - 1];
    }

    HYP_FORCE_INLINE const ValueType& Back() const
    {
        return data[size - 1];
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Size() == 0;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return Size() != 0;
    }

    HYP_FORCE_INLINE ValueType& operator[](KeyType index)
    {
        HYP_CORE_ASSERT(index >= 0 && index < Size(), "Index out of bounds");

        return data[index];
    }

    HYP_FORCE_INLINE const ValueType& operator[](KeyType index) const
    {
        HYP_CORE_ASSERT(index >= 0 && index < Size(), "Index out of bounds");

        return data[index];
    }

    void Reserve(uint32 capacity);

    void Resize(uint32 newSize);

    void ResizeUninitialized(uint32 newSize);

    void ResizeZeroed(uint32 newSize);

    void Refit();

    void SetCapacity(uint32 newCapacity);

    HYP_FORCE_INLINE uint32 Capacity() const
    {
        return capacity;
    }

    HYP_FORCE_INLINE ValueType& Add(const ValueType& value)
    {
        return PushBack(value);
    }

    HYP_FORCE_INLINE ValueType& Add(ValueType&& value)
    {
        return PushBack(std::move(value));
    }

    ValueType& PushBack(const ValueType& value);
    ValueType& PushBack(ValueType&& value);

    ValueType& PushFront(const ValueType& value);
    ValueType& PushFront(ValueType&& value);

    template <class... Args>
    ValueType& EmplaceBack(Args&&... args)
    {
        if (size + 1 > capacity)
        {
            SetCapacity(CalculateDesiredCapacity(size + 1));
        }

        TElemType* element = std::addressof(data[size++]);

        Memory::Construct<TElemType>(element, std::forward<Args>(args)...);

        return *element;
    }

    template <class... Args>
    ValueType& EmplaceFront(Args&&... args)
    {
        if (size + 1 > capacity)
        {
            SetCapacity(CalculateDesiredCapacity(size + 1));
        }

        ShiftElementsRight(1);

        TElemType* element = data;

        Memory::Construct<TElemType>(element, std::forward<Args>(args)...);

        return *element;
    }

    void Shift(uint32 count);

    HYP_NODISCARD SlimArray<TElemType, TAllocator> Slice(int first, int last) const;

    template <class TOtherAllocator>
    void Concat(const SlimArray<TElemType, TOtherAllocator>& other)
    {
        if ((void*)this == (void*)&other)
        {
            return;
        }

        if (other.Empty())
        {
            return;
        }

        Concat(Span<const TElemType>(other.Data(), other.Size()));
    }

    void Concat(Span<const TElemType> span)
    {
        const uint32 spanSize = uint32(span.Size());

        if (spanSize == 0)
        {
            return;
        }

        if (size + spanSize > capacity)
        {
            SetCapacity(CalculateDesiredCapacity(size + spanSize));
        }

        if constexpr (std::is_fundamental_v<TElemType> || std::is_trivially_copy_constructible_v<TElemType>)
        {
            Memory::Copy(data + size, span.Data(), spanSize * sizeof(TElemType));

            size += spanSize;
        }
        else
        {
            for (uint32 i = 0; i < spanSize; ++i)
            {
                Memory::Construct<TElemType>(std::addressof(data[size++]), span.Data()[i]);
            }
        }
    }

    void Reverse();

    template <class TOtherAllocator>
    void Reverse(SlimArray<TElemType, TOtherAllocator>& outArray) const
    {
        const uint32 sz = Size();

        if (sz < 2)
        {
            return;
        }

        outArray.ResizeUninitialized(sz);

        for (uint32 i = 0; i < sz; ++i)
        {
            Memory::Construct<TElemType>(&outArray.data[i], data[sz - 1 - i]);
        }
    }

    Iterator Erase(ConstIterator iter);
    Iterator Erase(const TElemType& value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType& value);
    Iterator Insert(ConstIterator where, ValueType&& value);

    ValueType PopFront();
    ValueType PopBack();

    void Clear();

    template <class TOtherAllocator>
    HYP_FORCE_INLINE bool operator==(const SlimArray<TElemType, TOtherAllocator>& other) const
    {
        if (this == &other)
        {
            return true;
        }

        if (Size() != other.Size())
        {
            return false;
        }

        if constexpr (std::is_fundamental_v<TElemType> || std::is_enum_v<TElemType>)
        {
            return Memory::Compare(Data(), other.Data(), ByteSize()) == 0;
        }

        auto it = Begin();
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (!(*it == *otherIt))
            {
                return false;
            }
        }

        return true;
    }

    template <class TOtherAllocator>
    HYP_FORCE_INLINE bool operator!=(const SlimArray<TElemType, TOtherAllocator>& other) const
    {
        if (this == &other)
        {
            return false;
        }

        if (Size() != other.Size())
        {
            return true;
        }

        if constexpr (std::is_fundamental_v<TElemType> || std::is_enum_v<TElemType>)
        {
            return Memory::Compare(Data(), other.Data(), ByteSize()) != 0;
        }

        auto it = Begin();
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (*it != *otherIt)
            {
                return true;
            }
        }

        return false;
    }

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<TElemType>()
    {
        return Span<TElemType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const TElemType>() const
    {
        return Span<const TElemType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE Span<TElemType> ToSpan()
    {
        return Span<TElemType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE Span<const TElemType> ToSpan() const
    {
        return Span<const TElemType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE ByteView ToByteView(uint32 offset = 0, uint32 sizeVal = ~0u)
    {
        if (offset >= Size())
        {
            return ByteView();
        }

        if (sizeVal > Size())
        {
            sizeVal = Size();
        }

        return ByteView(reinterpret_cast<ubyte*>(Data()) + offset, sizeVal * sizeof(TElemType));
    }

    HYP_NODISCARD HYP_FORCE_INLINE ConstByteView ToByteView(uint32 offset = 0, uint32 sizeVal = ~0u) const
    {
        if (offset >= Size())
        {
            return ConstByteView();
        }

        if (sizeVal > Size())
        {
            sizeVal = Size();
        }

        return ConstByteView(reinterpret_cast<const ubyte*>(Data()) + offset, sizeVal * sizeof(TElemType));
    }

    HYP_DEF_STL_BEGIN_END(data, data + size)

protected:
    HYP_FORCE_INLINE static TAllocator* GetAllocator()
    {
        return GetDefaultAllocatorInstance<TAllocator>();
    }

    void Allocate(uint32 count)
    {
        HYP_CORE_ASSERT(data == nullptr);
        HYP_CORE_ASSERT(count > 0);

        data = static_cast<TElemType*>(GetAllocator()->Allocate(count * sizeof(TElemType), alignof(TElemType)));
        HYP_CORE_ASSERT(data != nullptr);

        capacity = count;
    }

    void Free()
    {
        if (data != nullptr)
        {
            GetAllocator()->Free(data);
            data = nullptr;
        }

        capacity = 0;
    }

    void InitFromRangeCopy(const TElemType* begin, const TElemType* end)
    {
        HYP_CORE_ASSERT(end >= begin);

        const uint32 count = uint32(end - begin);

        HYP_CORE_ASSERT(capacity >= count);

        if constexpr (std::is_fundamental_v<TElemType> || std::is_trivial_v<TElemType>)
        {
            Memory::Copy(data, begin, count * sizeof(TElemType));
        }
        else
        {
            for (uint32 i = 0; i < count; i++)
            {
                Memory::Construct<TElemType>(&data[i], begin[i]);
            }
        }
    }

    void InitFromRangeMove(TElemType* begin, TElemType* end)
    {
        HYP_CORE_ASSERT(end >= begin);

        const uint32 count = uint32(end - begin);

        HYP_CORE_ASSERT(capacity >= count);

        if constexpr (std::is_fundamental_v<TElemType> || std::is_trivial_v<TElemType>)
        {
            Memory::Copy(data, begin, count * sizeof(TElemType));
        }
        else if constexpr (std::is_move_constructible_v<TElemType>)
        {
            for (uint32 i = 0; i < count; i++)
            {
                Memory::Construct<TElemType>(&data[i], std::move(begin[i]));
            }
        }
        else if constexpr (std::is_copy_constructible_v<TElemType>)
        {
            for (uint32 i = 0; i < count; i++)
            {
                Memory::Construct<TElemType>(&data[i], begin[i]);
            }
        }
        else
        {
            HYP_CORE_ASSERT(count == 0, "InitFromRangeMove: T is neither move nor copy constructible");
        }
    }

    void DestructElements()
    {
        if constexpr (!std::is_trivially_destructible_v<TElemType>)
        {
            for (uint32 i = size; i > 0;)
            {
                data[--i].~TElemType();
            }
        }
    }

    void ShiftElementsRight(uint32 offset)
    {
        HYP_CORE_ASSERT(size + offset <= capacity);

        for (uint32 i = size; i > 0;)
        {
            --i;

            if constexpr (std::is_move_constructible_v<TElemType>)
            {
                Memory::Construct<TElemType>(&data[i + offset], std::move(data[i]));
            }
            else
            {
                Memory::Construct<TElemType>(&data[i + offset], data[i]);
            }

            Memory::Destruct(data[i]);
        }

        size += offset;
    }

    void ShiftElementsLeft(uint32 count)
    {
        HYP_CORE_ASSERT(count <= size);

        const uint32 newSize = size - count;

        if constexpr (std::is_trivially_copyable_v<TElemType>)
        {
            Memory::Move(data, data + count, newSize * sizeof(TElemType));
        }
        else
        {
            for (uint32 i = 0; i < newSize; ++i)
            {
                if constexpr (std::is_move_assignable_v<TElemType>)
                {
                    data[i] = std::move(data[i + count]);
                }
                else if constexpr (std::is_move_constructible_v<TElemType>)
                {
                    Memory::Destruct(data[i]);
                    Memory::Construct<TElemType>(&data[i], std::move(data[i + count]));
                }
                else
                {
                    data[i] = data[i + count];
                }
            }

            // Destruct the tail elements after all moves are complete
            for (uint32 i = size; i > newSize;)
            {
                Memory::Destruct(data[--i]);
            }
        }

        size = newSize;
    }

    static uint32 CalculateDesiredCapacity(uint32 desiredSize)
    {
        return 1u << uint32(std::ceil(std::log(desiredSize) / std::log(2.0)));
    }

    TElemType* data;
    uint32 size;
    uint32 capacity;
};

template <class TElemType, class TAllocator>
SlimArray<TElemType, TAllocator>::SlimArray(const SlimArray& other)
    : data(nullptr),
      size(0),
      capacity(0)
{
    HYP_CORE_ASSERT(GetAllocator() != nullptr);

    size = other.size;

    if (size > 0)
    {
        Allocate(size);
        InitFromRangeCopy(other.Data(), other.Data() + other.size);
    }
}

template <class TElemType, class TAllocator>
SlimArray<TElemType, TAllocator>::SlimArray(SlimArray&& other) noexcept
    : data(nullptr),
      size(0),
      capacity(0)
{
    HYP_CORE_ASSERT(GetAllocator() != nullptr);

    data = other.data;
    size = other.size;
    capacity = other.capacity;

    other.data = nullptr;
    other.size = 0;
    other.capacity = 0;
}

template <class TElemType, class TAllocator>
SlimArray<TElemType, TAllocator>::~SlimArray()
{
    if (data != nullptr)
    {
        DestructElements();

        Free();
    }

    size = 0;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::operator=(const SlimArray& other) -> SlimArray&
{
    if (this == &other)
    {
        return *this;
    }

    DestructElements();
    Free();

    size = other.size;

    if (size > 0)
    {
        Allocate(size);
        InitFromRangeCopy(other.Data(), other.Data() + other.size);
    }

    return *this;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::operator=(SlimArray&& other) noexcept -> SlimArray&
{
    if (this == &other)
    {
        return *this;
    }

    DestructElements();
    Free();

    data = other.data;
    size = other.size;
    capacity = other.capacity;

    other.data = nullptr;
    other.size = 0;
    other.capacity = 0;

    return *this;
}

template <class TElemType, class TAllocator>
template <class TOtherAllocator, typename>
auto SlimArray<TElemType, TAllocator>::operator=(const SlimArray<TElemType, TOtherAllocator>& other) -> SlimArray&
{
    DestructElements();
    Free();

    size = other.size;

    if (size > 0)
    {
        Allocate(size);
        InitFromRangeCopy(other.Data(), other.Data() + other.size);
    }

    return *this;
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Reserve(uint32 capacityValue)
{
    if (capacity >= capacityValue)
    {
        return;
    }

    SetCapacity(capacityValue);
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Resize(uint32 newSize)
{
    if (newSize == size)
    {
        return;
    }

    if (newSize > size)
    {
        const uint32 diff = newSize - size;

        if (size + diff > capacity)
        {
            SetCapacity(CalculateDesiredCapacity(size + diff));
        }

        if constexpr (std::is_fundamental_v<TElemType> || std::is_trivially_constructible_v<TElemType>)
        {
            Memory::Zero(data + size, sizeof(TElemType) * diff);

            size += diff;
        }
        else
        {
            while (size < newSize)
            {
                Memory::Construct<TElemType>(std::addressof(data[size++]));
            }
        }
    }
    else
    {
        const uint32 diff = size - newSize;

        for (uint32 i = size; i > size - diff;)
        {
            Memory::Destruct(data[--i]);
        }

        size -= diff;
    }
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::ResizeUninitialized(uint32 newSize)
{
    if (newSize == size)
    {
        return;
    }

    if (newSize > size)
    {
        const uint32 diff = newSize - size;

        if (size + diff > capacity)
        {
            SetCapacity(CalculateDesiredCapacity(size + diff));
        }

        size += diff;
    }
    else
    {
        const uint32 diff = size - newSize;

        for (uint32 i = size; i > size - diff;)
        {
            Memory::Destruct(data[--i]);
        }

        size -= diff;
    }
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::ResizeZeroed(uint32 newSize)
{
    static_assert(std::is_fundamental_v<TElemType> || std::is_trivially_constructible_v<TElemType>,
        "ResizeZeroed can only be used for fundamental or trivially constructible types");

    if (newSize == size)
    {
        return;
    }

    const uint32 currentSize = size;

    ResizeUninitialized(newSize);

    if (newSize > currentSize)
    {
        Memory::Zero(data + currentSize, sizeof(TElemType) * (newSize - currentSize));
    }
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Refit()
{
    if (capacity == size)
    {
        return;
    }

    SetCapacity(size);
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::SetCapacity(uint32 newCapacity)
{
    if (newCapacity == capacity)
    {
        return;
    }

    HYP_CORE_ASSERT(newCapacity <= SIZE_MAX / sizeof(TElemType));

    TElemType* newData = nullptr;
    const uint32 copySize = MathUtil::Min(size, newCapacity);

    if (newCapacity > 0)
    {
        newData = static_cast<TElemType*>(GetAllocator()->Allocate(newCapacity * sizeof(TElemType), alignof(TElemType)));
        HYP_CORE_ASSERT(newData != nullptr);

        if (copySize > 0)
        {
            if constexpr (std::is_fundamental_v<TElemType> || std::is_trivial_v<TElemType>)
            {
                Memory::Copy(newData, data, copySize * sizeof(TElemType));
            }
            else
            {
                for (uint32 i = 0; i < copySize; i++)
                {
                    Memory::Construct<TElemType>(&newData[i], std::move(data[i]));
                }
            }
        }
    }

    DestructElements();
    Free();

    data = newData;
    capacity = newCapacity;
    size = copySize;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PushBack(const ValueType& value) -> ValueType&
{
    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    TElemType* element = std::addressof(data[size++]);

    Memory::Construct<TElemType>(element, value);

    return *element;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PushBack(ValueType&& value) -> ValueType&
{
    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    TElemType* element = std::addressof(data[size++]);

    Memory::Construct<TElemType>(element, std::move(value));

    return *element;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PushFront(const ValueType& value) -> ValueType&
{
    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    ShiftElementsRight(1);

    TElemType* element = data;

    Memory::Construct<TElemType>(element, value);

    return *element;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PushFront(ValueType&& value) -> ValueType&
{
    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    ShiftElementsRight(1);

    TElemType* element = data;

    Memory::Construct<TElemType>(element, std::move(value));

    return *element;
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Shift(uint32 count)
{
    ShiftElementsLeft(count);
}

template <class TElemType, class TAllocator>
SlimArray<TElemType, TAllocator> SlimArray<TElemType, TAllocator>::Slice(int first, int last) const
{
    if (first < 0)
    {
        first = Size() + first;
    }

    if (last < 0)
    {
        last = Size() + last;
    }

    if (first < 0)
    {
        first = 0;
    }

    if (last < 0)
    {
        last = 0;
    }

    if (first > last)
    {
        return SlimArray<TElemType, TAllocator>();
    }

    if (first >= int(Size()))
    {
        return SlimArray<TElemType, TAllocator>();
    }

    if (last >= int(Size()))
    {
        last = Size() - 1;
    }

    SlimArray<TElemType, TAllocator> result;
    result.ResizeUninitialized(last - first + 1);

    for (uint32 i = 0; i < result.size; ++i)
    {
        Memory::Construct<TElemType>(&result.data[i], data[first + i]);
    }

    return result;
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Reverse()
{
    if (size < 2)
    {
        return;
    }

    uint32 left = 0;
    uint32 right = size - 1;

    while (left < right)
    {
        std::swap(data[left], data[right]);

        ++left;
        --right;
    }
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::Erase(ConstIterator iter) -> Iterator
{
    const Iterator begin = Begin();
    const Iterator end = End();

    if (iter < begin || iter >= end)
    {
        return end;
    }

    const uint32 dist = uint32(iter - begin);

    if constexpr (std::is_trivially_copyable_v<TElemType>)
    {
        TElemType* erasePtr = data + dist;
        const uint32 numToMove = size - dist - 1;

        if (numToMove > 0)
        {
            Memory::Move(erasePtr, erasePtr + 1, numToMove * sizeof(TElemType));
        }
    }
    else
    {
        for (uint32 index = dist; index < size - 1; ++index)
        {
            if constexpr (std::is_move_constructible_v<TElemType>)
            {
                Memory::Destruct(data[index]);
                Memory::Construct<TElemType>(&data[index], std::move(data[index + 1]));
            }
            else
            {
                Memory::Destruct(data[index]);
                Memory::Construct<TElemType>(&data[index], data[index + 1]);
            }
        }

        Memory::Destruct(data[size - 1]);
    }

    --size;

    return begin + dist;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::Erase(const TElemType& value) -> Iterator
{
    ConstIterator iter = Base::Find(value);

    if (iter != End())
    {
        return Erase(iter);
    }

    return End();
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::EraseAt(typename Base::KeyType index) -> Iterator
{
    return Erase(Begin() + index);
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::Insert(ConstIterator where, const ValueType& value) -> Iterator
{
    const uint32 dist = uint32(where - Begin());

    if (where == End())
    {
        PushBack(value);

        return &data[size - 1];
    }

    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    if constexpr (std::is_trivially_copyable_v<TElemType>)
    {
        TElemType* insertPtr = data + dist;
        const uint32 numToMove = size - dist;

        if (numToMove > 0)
        {
            Memory::Move(insertPtr + 1, insertPtr, numToMove * sizeof(TElemType));
        }

        Memory::Construct<TElemType>(insertPtr, value);
    }
    else
    {
        uint32 index;

        for (index = size; index > dist; --index)
        {
            if constexpr (std::is_move_constructible_v<TElemType>)
            {
                Memory::Construct<TElemType>(&data[index], std::move(data[index - 1]));
            }
            else
            {
                Memory::Construct<TElemType>(&data[index], data[index - 1]);
            }

            Memory::Destruct(data[index - 1]);
        }

        Memory::Construct<TElemType>(&data[index], value);
    }

    ++size;

    return Begin() + dist;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::Insert(ConstIterator where, ValueType&& value) -> Iterator
{
    const uint32 dist = uint32(where - Begin());

    if (where == End())
    {
        PushBack(std::move(value));

        return &data[size - 1];
    }

    if (size + 1 > capacity)
    {
        SetCapacity(CalculateDesiredCapacity(size + 1));
    }

    if constexpr (std::is_trivially_copyable_v<TElemType>)
    {
        TElemType* insertPtr = data + dist;
        const uint32 numToMove = size - dist;

        if (numToMove > 0)
        {
            Memory::Move(insertPtr + 1, insertPtr, numToMove * sizeof(TElemType));
        }

        Memory::Construct<TElemType>(insertPtr, std::move(value));
    }
    else
    {
        uint32 index;

        for (index = size; index > dist; --index)
        {
            if constexpr (std::is_move_constructible_v<TElemType>)
            {
                Memory::Construct<TElemType>(&data[index], std::move(data[index - 1]));
            }
            else
            {
                Memory::Construct<TElemType>(&data[index], data[index - 1]);
            }

            Memory::Destruct(data[index - 1]);
        }

        Memory::Construct<TElemType>(&data[index], std::move(value));
    }

    ++size;

    return Begin() + dist;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PopFront() -> ValueType
{
    HYP_CORE_ASSERT(size != 0);

    auto value = std::move(data[0]);

    ShiftElementsLeft(1);

    return value;
}

template <class TElemType, class TAllocator>
auto SlimArray<TElemType, TAllocator>::PopBack() -> ValueType
{
    HYP_CORE_ASSERT(size != 0);

    auto value = std::move(data[size - 1]);

    Memory::Destruct(data[size - 1]);

    --size;

    return value;
}

template <class TElemType, class TAllocator>
void SlimArray<TElemType, TAllocator>::Clear()
{
    DestructElements();

    size = 0;

    Refit();
}

} // namespace containers

using containers::SlimArray;

// traits

template <class T, class TAllocator>
struct IsArray<containers::SlimArray<T, TAllocator>> : std::true_type
{
};

} // namespace Hyperion
