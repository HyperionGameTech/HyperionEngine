/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace containers {

/*! \brief FIFO stack based on Array<T> class. */
template <class T>
class Stack : Array<T>
{
public:
    using Base = Array<T>;

    using KeyType = typename Base::KeyType;
    using ValueType = typename Base::ValueType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    Stack();
    Stack(const Stack& other);
    Stack(Stack&& other) noexcept;
    ~Stack();

    Stack& operator=(const Stack& other);
    Stack& operator=(Stack&& other) noexcept;

    HYP_FORCE_INLINE size_t Size() const
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

    HYP_FORCE_INLINE typename Base::ValueType& Top()
    {
        return Base::Back();
    }

    HYP_FORCE_INLINE const typename Base::ValueType& Top() const
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

    HYP_FORCE_INLINE void Reserve(size_t capacity)
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
Stack<T>::Stack()
    : Base()
{
}

template <class T>
Stack<T>::Stack(const Stack& other)
    : Base(other)
{
}

template <class T>
Stack<T>::Stack(Stack&& other) noexcept
    : Base(std::move(other))
{
}

template <class T>
Stack<T>::~Stack()
{
}

template <class T>
auto Stack<T>::operator=(const Stack& other) -> Stack&
{
    Base::operator=(other);

    return *this;
}

template <class T>
auto Stack<T>::operator=(Stack&& other) noexcept -> Stack&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
auto Stack<T>::Push(const typename Base::ValueType& value) -> typename Base::ValueType&
{
    return Base::PushBack(value);
}

template <class T>
auto Stack<T>::Push(typename Base::ValueType&& value) -> typename Base::ValueType&
{
    return Base::PushBack(std::move(value));
}

template <class T>
auto Stack<T>::Pop() -> typename Base::ValueType
{
    return Base::PopBack();
}

template <class T>
void Stack<T>::Clear()
{
    Base::Clear();
}
} // namespace containers

template <class T>
using Stack = containers::Stack<T>;

} // namespace Hyperion
