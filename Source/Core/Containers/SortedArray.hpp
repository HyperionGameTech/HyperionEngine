/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/ContainerBase.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Pair.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {
namespace containers {

template <class T, class AllocatorType = DynamicAllocator>
class TSortedArray : public Array<T, AllocatorType>
{
protected:
    using Base = Array<T, AllocatorType>;
    using KeyType = typename Base::KeyType;
    using ValueType = typename Base::ValueType;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    TSortedArray();

    TSortedArray(std::initializer_list<T> initializerList)
        : Base(initializerList)
    {
        std::sort(Begin(), End());
    }

    TSortedArray(const T* begin, const T* end)
        : Base(begin, end)
    {
        std::sort(Begin(), End());
    }

    TSortedArray(const TSortedArray& other);
    TSortedArray& operator=(const TSortedArray& other);
    
    TSortedArray(TSortedArray&& other) noexcept;
    TSortedArray& operator=(TSortedArray&& other) noexcept;

    ~TSortedArray();

    Iterator Find(const T& value);
    ConstIterator Find(const T& value) const;

    Iterator Insert(const T& value);
    Iterator Insert(T&& value);

    /*! \brief Performs a direct call to Array::Erase(), erasing the element at the iterator position. */
    Iterator Erase(ConstIterator it)
    {
        return Base::Erase(it);
    }

    /*! \brief Erase an element by value. The item is searched for using binary search,
        and if the item was found, it will be erased (and iterators will be invalidated) */
    Iterator Erase(const T& value);

    size_t Size() const
    {
        return Base::Size();
    }

    T* Data()
    {
        return Base::Data();
    }

    const T* Data() const
    {
        return Base::Data();
    }

    bool Empty() const
    {
        return Base::Empty();
    }

    bool Any() const
    {
        return Base::Any();
    }

    bool Contains(const T& value) const
    {
        return Find(value) != End();
    }

    void Clear()
    {
        Base::Clear();
    }

    T& Front()
    {
        return Base::Front();
    }

    const T& Front() const
    {
        return Base::Front();
    }

    T& Back()
    {
        return Base::Back();
    }

    const T& Back() const
    {
        return Base::Back();
    }

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End())

private:
    // Make these Array<T> methods private, so that they can't be used to break the sorted invariant
    using Base::Concat;
    using Base::PopBack;
    using Base::PopFront;
    using Base::PushBack;
    using Base::PushFront;
};

template <class T, class AllocatorType>
TSortedArray<T, AllocatorType>::TSortedArray()
    : Base()
{
}

template <class T, class AllocatorType>
TSortedArray<T, AllocatorType>::TSortedArray(const TSortedArray& other)
    : Base(other)
{
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::operator=(const TSortedArray& other) -> TSortedArray&
{
    Base::operator=(other);

    return *this;
}

template <class T, class AllocatorType>
TSortedArray<T, AllocatorType>::TSortedArray(TSortedArray&& other) noexcept
    : Base(std::move(other))
{
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::operator=(TSortedArray&& other) noexcept -> TSortedArray&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T, class AllocatorType>
TSortedArray<T, AllocatorType>::~TSortedArray() = default;

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::Find(const T& value) -> Iterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::Find(const T& value) const -> ConstIterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::Insert(const T& value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, value);
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::Insert(T&& value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, std::forward<T>(value));
}

template <class T, class AllocatorType>
auto TSortedArray<T, AllocatorType>::Erase(const T& value) -> Iterator
{
    const ConstIterator iter = Base::Find(value);

    if (iter == End())
    {
        return End();
    }

    return Base::Erase(iter);
}

} // namespace containers

using containers::TSortedArray;

} // namespace Hyperion
