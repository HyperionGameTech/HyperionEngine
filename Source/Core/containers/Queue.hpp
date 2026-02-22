/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/Defines.hpp>

#include <core/Types.hpp>

namespace Hyperion {
namespace containers {

/*! \brief FILO queue based on Array<T> class. */
template <class T>
class Queue : Array<T>
{
public:
    using Base = Array<T>;

    using KeyType = typename Base::KeyType;
    using ValueType = typename Base::ValueType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    Queue();
    Queue(const Queue& other);
    Queue(Queue&& other) noexcept;
    ~Queue();

    Queue& operator=(const Queue& other);
    Queue& operator=(Queue&& other) noexcept;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return Base::Size();
    }

    HYP_FORCE_INLINE typename Base::ValueType* Data()
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE const typename Base::ValueType* Data() const
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE typename Base::ValueType& Front()
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE const typename Base::ValueType& Front() const
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE typename Base::ValueType& Back()
    {
        return Base::Back();
    }

    HYP_FORCE_INLINE const typename Base::ValueType& Back() const
    {
        return Base::Back();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Base::Empty();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return Base::Any();
    }

    HYP_FORCE_INLINE bool Contains(const T& value) const
    {
        return Base::Contains(value);
    }

    HYP_FORCE_INLINE void Reserve(SizeType capacity)
    {
        Base::Reserve(capacity);
    }

    HYP_FORCE_INLINE void Refit()
    {
        Base::Refit();
    }

    /*! \brief Alias for Push(). */
    HYP_FORCE_INLINE typename Base::ValueType& Add(const typename Base::ValueType& value)
    {
        return Push(value);
    }

    /*! \brief Alias for Push(). */
    HYP_FORCE_INLINE typename Base::ValueType& Add(typename Base::ValueType&& value)
    {
        return Push(std::move(value));
    }

    typename Base::ValueType& Push(const typename Base::ValueType& value);
    typename Base::ValueType& Push(typename Base::ValueType&& value);

    typename Base::ValueType Pop();

    void Clear();

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End())
};

template <class T>
Queue<T>::Queue()
    : Base()
{
}

template <class T>
Queue<T>::Queue(const Queue& other)
    : Base(other)
{
}

template <class T>
Queue<T>::Queue(Queue&& other) noexcept
    : Base(std::move(other))
{
}

template <class T>
Queue<T>::~Queue()
{
}

template <class T>
auto Queue<T>::operator=(const Queue& other) -> Queue&
{
    Base::operator=(other);

    return *this;
}

template <class T>
auto Queue<T>::operator=(Queue&& other) noexcept -> Queue&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
auto Queue<T>::Push(const typename Base::ValueType& value) -> typename Base::ValueType&
{
    return Base::PushBack(value);
}

template <class T>
auto Queue<T>::Push(typename Base::ValueType&& value) -> typename Base::ValueType&
{
    return Base::PushBack(std::move(value));
}

template <class T>
auto Queue<T>::Pop() -> typename Base::ValueType
{
    return Base::PopFront();
}

template <class T>
void Queue<T>::Clear()
{
    Base::Clear();
}

} // namespace containers

template <class T>
using Queue = containers::Queue<T>;

} // namespace Hyperion
