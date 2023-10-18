#ifndef HYPERION_V2_LIB_FLAT_SET_H
#define HYPERION_V2_LIB_FLAT_SET_H

#include "ContainerBase.hpp"
#include "SortedArray.hpp"
#include "Pair.hpp"
#include <util/Defines.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

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
        : SortedArray<T>(initializer_list)
    {
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

    [[nodiscard]] SizeType Size() const    { return Base::Size(); }
    [[nodiscard]] T *Data()                               { return Base::Data(); }
    [[nodiscard]] T * const Data() const                  { return Base::Data(); }
    [[nodiscard]] bool Any() const                        { return Base::Any(); }
    [[nodiscard]] bool Empty() const                      { return Base::Empty(); }
    [[nodiscard]] bool Contains(const T &value) const     { return Find(value) != End(); }
    void Clear()                                          { Base::Clear(); }
    
    [[nodiscard]] T &Front()                              { return Base::Front(); }
    [[nodiscard]] const T &Front() const                  { return Base::Front(); }
    [[nodiscard]] T &Back()                               { return Base::Back(); }
    [[nodiscard]] const T &Back() const                   { return Base::Back(); }

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

    Array<T> ToArray() const
    {
        Array<T> result;
        result.Reserve(Size());

        for (const auto &it : *this) {
            result.PushBack(it);
        }

        return result;
    }

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

} // namespace hyperion

#endif
