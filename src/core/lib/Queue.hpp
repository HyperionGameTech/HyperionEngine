#ifndef HYPERION_V2_LIB_QUEUE_H
#define HYPERION_V2_LIB_QUEUE_H

#include "DynArray.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

/*! Queue based on Base. About 30% faster than std::queue in my tests. */

template <class T>
class Queue : Array<T>
{
public:
    using Base = Array<T>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    Queue();
    Queue(const Queue &other);
    Queue(Queue &&other) noexcept;
    ~Queue();

    Queue &operator=(const Queue &other);
    Queue &operator=(Queue &&other) noexcept;

    [[nodiscard]] SizeType Size() const
        { return Base::Size(); }

    [[nodiscard]] typename Base::ValueType *Data()
        { return Base::Data(); }

    [[nodiscard]] const typename Base::ValueType *Data() const
        { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Front()
        { return Base::Front(); }

    [[nodiscard]] const typename Base::ValueType &Front() const
        { return Base::Front(); }

    [[nodiscard]] typename Base::ValueType &Back()
        { return Base::Back(); }

    [[nodiscard]] const typename Base::ValueType &Back() const
        { return Base::Back(); }

    [[nodiscard]] bool Empty() const
        { return Base::Empty(); }

    [[nodiscard]] bool Any() const
        { return Base::Any(); }

    [[nodiscard]] bool Contains(const T &value) const
        { return Base::Contains(value); }

    void Reserve(SizeType capacity)
        { Base::Reserve(capacity); }

    void Refit()
        { Base::Refit(); }

    void Push(const typename Base::ValueType &value);
    void Push(typename Base::ValueType &&value);
    typename Base::ValueType Pop();
    void Clear();

    HYP_DEF_STL_BEGIN_END(
        Base::Begin(),
        Base::End()
    )
};

template <class T>
Queue<T>::Queue()
    : Base()
{
}

template <class T>
Queue<T>::Queue(const Queue &other)
    : Base(other)
{
}

template <class T>
Queue<T>::Queue(Queue &&other) noexcept
    : Base(std::move(other))
{
}

template <class T>
Queue<T>::~Queue()
{
}

template <class T>
auto Queue<T>::operator=(const Queue &other) -> Queue&
{
    Base::operator=(other);

    return *this;
}

template <class T>
auto Queue<T>::operator=(Queue &&other) noexcept -> Queue&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
void Queue<T>::Push(const typename Base::ValueType &value)
{
    Base::PushBack(value);
}

template <class T>
void Queue<T>::Push(typename Base::ValueType &&value)
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

} // namespace hyperion

#endif