#ifndef HYPERION_V2_LIB_SORTED_ARRAY_H
#define HYPERION_V2_LIB_SORTED_ARRAY_H

#include "ContainerBase.hpp"
#include "DynArray.hpp"
#include "Pair.hpp"
#include <util/Defines.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class T>
class SortedArray : public Array<T>
{
protected:
    using Base = Array<T>;
    using ValueType = typename Base::ValueType;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    SortedArray();

    SortedArray(std::initializer_list<T> initializer_list)
        : Base(initializer_list)
    {
    }

    SortedArray(const SortedArray &other);
    SortedArray &operator=(const SortedArray &other);
    SortedArray(SortedArray &&other) noexcept;
    SortedArray &operator=(SortedArray &&other) noexcept;
    ~SortedArray();
    
    [[nodiscard]] Iterator Find(const T &value);
    [[nodiscard]] ConstIterator Find(const T &value) const;
    
    Iterator Insert(const T &value);
    Iterator Insert(T &&value);

    //! \brief Performs a direct call to DynArray::Erase(), erasing the element at the iterator
    //  position.
    Iterator Erase(ConstIterator it)                      { return Base::Erase(it); }
    //! \brief Erase an element by value. The item is searched for using binary search,
    //  and if the item was found, it will be erased (and iterators will be invalidated)
    Iterator Erase(const T &value);

    [[nodiscard]] SizeType Size() const                   { return Base::Size(); }
    [[nodiscard]] T *Data()                               { return Base::Data(); }
    [[nodiscard]] T * const Data() const                  { return Base::Data(); }
    [[nodiscard]] bool Empty() const                      { return Base::Empty(); }
    [[nodiscard]] bool Any() const                        { return Base::Any(); }
    [[nodiscard]] bool Contains(const T &value) const     { return Find(value) != End(); }
    void Clear()                                          { Base::Clear(); }
    
    [[nodiscard]] T &Front()                              { return Base::Front(); }
    [[nodiscard]] const T &Front() const                  { return Base::Front(); }
    [[nodiscard]] T &Back()                               { return Base::Back(); }
    [[nodiscard]] const T &Back() const                   { return Base::Back(); }

    HYP_DEF_STL_BEGIN_END(
        Base::Begin(),
        Base::End()
    )

private:
    // Make these Array<T> methods private, so that they can't be used to break the sorted invariant
    using Base::PushBack;
    using Base::PushFront;
    using Base::PopBack;
    using Base::PopFront;
    using Base::Concat;
};

template <class T>
SortedArray<T>::SortedArray()
    : Base()
{
    
}

template <class T>
SortedArray<T>::SortedArray(const SortedArray &other)
    : Base(other)
{
}

template <class T>
auto SortedArray<T>::operator=(const SortedArray &other) -> SortedArray&
{
    Base::operator=(other);

    return *this;
}

template <class T>
SortedArray<T>::SortedArray(SortedArray &&other) noexcept
    : Base(std::move(other))
{
}

template <class T>
auto SortedArray<T>::operator=(SortedArray &&other) noexcept -> SortedArray&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
SortedArray<T>::~SortedArray() = default;

template <class T>
auto SortedArray<T>::Find(const T &value) -> Iterator
{
    const auto it = Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Find(const T &value) const -> ConstIterator
{
    const auto it = Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Insert(const T &value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, value);
}

template <class T>
auto SortedArray<T>::Insert(T &&value) -> Iterator
{
    Iterator it = Base::LowerBound(value);

    return Base::Insert(it, std::forward<T>(value));
}

template <class T>
auto SortedArray<T>::Erase(const T &value) -> Iterator
{
    const ConstIterator iter = Base::Find(value);

    if (iter == End()) {
        return End();
    }

    return Base::Erase(iter);
}

} // namespace hyperion

#endif
