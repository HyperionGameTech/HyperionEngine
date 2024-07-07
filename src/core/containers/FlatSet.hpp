/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FLAT_SET_HPP
#define HYPERION_FLAT_SET_HPP

#include <core/containers/ContainerBase.hpp>
#include <core/containers/SortedArray.hpp>
#include <core/utilities/Pair.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace containers {

template <class T>
class FlatSet : public SortedArray<T>
{
protected:
    using Base = SortedArray<T>;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    FlatSet();
    FlatSet(std::initializer_list<T> initializer_list)
        : Base()
    {
        for (const auto &item : initializer_list) {
            Insert(item);
        }
    }

    FlatSet(const T *begin, const T *end)
    {
        for (const T *it = begin; it != end; ++it) {
            Insert(*it);
        }
    }

    FlatSet(const FlatSet &other);
    FlatSet &operator=(const FlatSet &other);
    FlatSet(FlatSet &&other) noexcept;
    FlatSet &operator=(FlatSet &&other) noexcept;
    ~FlatSet();
    
    [[nodiscard]] Iterator Find(const T &value);
    [[nodiscard]] ConstIterator Find(const T &value) const;
    
    InsertResult Insert(const T &value);
    InsertResult Insert(T &&value);

    template <class ...Args>
    InsertResult Emplace(Args &&... args)
        { return Insert(T(std::forward<Args>(args)...)); }

    Iterator Erase(ConstIterator it);
    Iterator Erase(const T &value);

    [[nodiscard]] SizeType Size() const                 { return Base::Size(); }
    [[nodiscard]] T *Data()                             { return Base::Data(); }
    [[nodiscard]] const T *Data() const                 { return Base::Data(); }
    [[nodiscard]] bool Any() const                      { return Base::Any(); }
    [[nodiscard]] bool Empty() const                    { return Base::Empty(); }
    [[nodiscard]] bool Contains(const T &value) const   { return Find(value) != End(); }

    void Clear()                                        { Base::Clear(); }
    void Reserve(SizeType size)                         { Base::Reserve(size); }
    
    HYP_FORCE_INLINE
    T &Front()
        { return Base::Front(); }
    HYP_FORCE_INLINE
    const T &Front() const
        { return Base::Front(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    T &Back()
        { return Base::Back(); }
        
    HYP_NODISCARD HYP_FORCE_INLINE
    const T &Back() const
        { return Base::Back(); }

    template <class OtherContainerType>
    void Merge(const OtherContainerType &other)
    {
        for (const auto &item : other) {
            Insert(item);
        }
    }

    template <class OtherContainerType>
    void Merge(OtherContainerType &&other)
    {
        for (auto &item : other) {
            Insert(std::move(item));
        }

        other.Clear();
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    Array<T> ToArray() const
        { return Array<T>(Begin(), End()); }

    HYP_DEF_STL_BEGIN_END(
        Base::Begin(),
        Base::End()
    )
};

template <class T>
FlatSet<T>::FlatSet()
    : Base()
{
}

template <class T>
FlatSet<T>::FlatSet(const FlatSet &other)
    : Base(other)
{
}

template <class T>
auto FlatSet<T>::operator=(const FlatSet &other) -> FlatSet&
{
    Base::operator=(other);

    return *this;
}

template <class T>
FlatSet<T>::FlatSet(FlatSet &&other) noexcept
    : Base(std::move(other))
{
}

template <class T>
auto FlatSet<T>::operator=(FlatSet &&other) noexcept -> FlatSet&
{
    Base::operator=(std::move(other));

    return *this;
}

template <class T>
FlatSet<T>::~FlatSet() = default;

template <class T>
auto FlatSet<T>::Find(const T &value) -> Iterator
{
    const auto it = Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Find(const T &value) const -> ConstIterator
{
    const auto it = Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Insert(const T &value) -> InsertResult
{
    Iterator it = Base::LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = Base::Base::Insert(it, value);

        return {it, true};
    }

    return {it, false};
}

template <class T>
auto FlatSet<T>::Insert(T &&value) -> InsertResult
{
    Iterator it = Base::LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = Base::Base::Insert(it, std::forward<T>(value));

        return {it, true};
    }

    return {it, false};
}

template <class T>
auto FlatSet<T>::Erase(ConstIterator it) -> Iterator
{
    return Base::Erase(it);
}

template <class T>
auto FlatSet<T>::Erase(const T &value) -> Iterator
{
    const auto it = Find(value);

    if (it == End()) {
        return End();
    }

    return Base::Erase(it);
}

} // namespace containers

template <class T>
using FlatSet = containers::FlatSet<T>;

} // namespace hyperion

#endif
