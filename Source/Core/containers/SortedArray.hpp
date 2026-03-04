/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/ContainerBase.hpp>
#include <Core/containers/Array.hpp>
#include <Core/utilities/Pair.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {
namespace containers {

template <class T>
class SortedArray : public Array<T>
{
protected:
    using Base = Array<T>;
    using KeyType = typename Base::KeyType;
    using ValueType = typename Base::ValueType;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    SortedArray();

    SortedArray(std::initializer_list<T> initializerList)
        : Base(initializerList)
    {
        std::sort(Begin(), End());
    }

    SortedArray(const T* begin, const T* end)
        : Base(begin, end)
    {
        std::sort(Begin(), End());
    }

    SortedArray(const SortedArray& other);
    SortedArray& operator=(const SortedArray& other);
    SortedArray(SortedArray&& other) noexcept;
    SortedArray& operator=(SortedArray&& other) noexcept;
    ~SortedArray();

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

template <class T>
SortedArray<T>::SortedArray()
    : Base()
{
}

template <class T>
SortedArray<T>::SortedArray(const SortedArray& other)
    : Base(other)
{
}

template <class T>
auto SortedArray<T>::operator=(const SortedArray& other) -> SortedArray&
{
    Base::operator=(other);

    return *this;
}

template <class T>
SortedArray<T>::SortedArray(SortedArray&& other) noexcept
    : Base(std::move(other))
{
}

template <class T>
auto SortedArray<T>::operator=(SortedArray&& other) noexcept -> SortedArray&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
SortedArray<T>::~SortedArray() = default;

template <class T>
auto SortedArray<T>::Find(const T& value) -> Iterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Find(const T& value) const -> ConstIterator
{
    const auto it = Base::LowerBound(value);

    if (it == End())
    {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Insert(const T& value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, value);
}

template <class T>
auto SortedArray<T>::Insert(T&& value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, std::forward<T>(value));
}

template <class T>
auto SortedArray<T>::Erase(const T& value) -> Iterator
{
    const ConstIterator iter = Base::Find(value);

    if (iter == End())
    {
        return End();
    }

    return Base::Erase(iter);
}

} // namespace containers

template <class T>
using SortedArray = containers::SortedArray<T>;

} // namespace Hyperion
