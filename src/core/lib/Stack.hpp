#ifndef HYPERION_V2_LIB_STACK_HPP
#define HYPERION_V2_LIB_STACK_HPP

#include <core/lib/DynArray.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T>
class Stack : Array<T>
{
public:
    using Base = Array<T>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    Stack();
    Stack(const Stack &other);
    Stack(Stack &&other) noexcept;
    ~Stack();

    Stack &operator=(const Stack &other);
    Stack &operator=(Stack &&other) noexcept;

    [[nodiscard]] SizeType Size() const
        { return Base::Size(); }

    [[nodiscard]] typename Base::ValueType *Data()
        { return Base::Data(); }

    [[nodiscard]] const typename Base::ValueType *Data() const
        { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Top()
        { return Base::Back(); }

    [[nodiscard]] const typename Base::ValueType &Top() const
        { return Base::Back(); }

    [[nodiscard]] bool Empty() const
        { return Base::Empty(); }

    [[nodiscard]] bool Any() const
        { return Base::Any(); }

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
Stack<T>::Stack()
    : Base()
{
}

template <class T>
Stack<T>::Stack(const Stack &other)
    : Base(other)
{
}

template <class T>
Stack<T>::Stack(Stack &&other) noexcept
    : Base(std::move(other))
{
}

template <class T>
Stack<T>::~Stack()
{
}

template <class T>
auto Stack<T>::operator=(const Stack &other) -> Stack &
{
    Base::operator=(other);

    return *this;
}

template <class T>
auto Stack<T>::operator=(Stack &&other) noexcept -> Stack &
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
void Stack<T>::Push(const typename Base::ValueType &value)
{
    Base::PushBack(value);
}

template <class T>
void Stack<T>::Push(typename Base::ValueType &&value)
{
    Base::PushBack(std::move(value));
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

} // namespace hyperion

#endif