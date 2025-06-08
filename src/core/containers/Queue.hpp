/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_QUEUE_HPP
#define HYPERION_QUEUE_HPP

#include <core/containers/Array.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
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

    [[nodiscard]]
    HYP_FORCE_INLINE SizeType Size() const
    {
        return Base::Size();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
        typename Base::ValueType*
        Data()
    {
        return Base::Data();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE const typename Base::ValueType* Data() const
    {
        return Base::Data();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
        typename Base::ValueType&
        Front()
    {
        return Base::Front();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE const typename Base::ValueType& Front() const
    {
        return Base::Front();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
        typename Base::ValueType&
        Back()
    {
        return Base::Back();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE const typename Base::ValueType& Back() const
    {
        return Base::Back();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE bool Empty() const
    {
        return Base::Empty();
    }

    [[nodiscard]]
    HYP_FORCE_INLINE bool Any() const
    {
        return Base::Any();
    }

    [[nodiscard]]
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

    void Push(const typename Base::ValueType& value);
    void Push(typename Base::ValueType&& value);
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
void Queue<T>::Push(const typename Base::ValueType& value)
{
    Base::PushBack(value);
}

template <class T>
void Queue<T>::Push(typename Base::ValueType&& value)
{
    Base::PushBack(std::move(value));
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

} // namespace hyperion

#endif