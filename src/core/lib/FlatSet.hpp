#ifndef HYPERION_V2_LIB_FLAT_SET_H
#define HYPERION_V2_LIB_FLAT_SET_H

#include "ContainerBase.hpp"
#include "DynArray.hpp"
#include <util/Defines.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class T>
class FlatSet : public ContainerBase<FlatSet<T>, T> {
    DynArray<T> m_vector;

public:
    using Iterator      = typename decltype(m_vector)::Iterator;
    using ConstIterator = typename decltype(m_vector)::ConstIterator;
    using InsertResult  = std::pair<Iterator, bool>; // iterator, was inserted

    FlatSet();
    FlatSet(std::initializer_list<T> initializer_list)
        : m_vector(initializer_list)
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

    bool Erase(Iterator it);
    bool Erase(const T &value);

    [[nodiscard]] size_t Size() const                     { return m_vector.Size(); }
    [[nodiscard]] T *Data()                               { return m_vector.Data(); }
    [[nodiscard]] T * const Data() const                  { return m_vector.Data(); }
    [[nodiscard]] bool Empty() const                      { return m_vector.Empty(); }
    [[nodiscard]] bool Contains(const T &value) const     { return Find(value) != End(); }
    void Clear()                                          { m_vector.Clear(); }
    
    [[nodiscard]] T &Front()                              { return m_vector.Front(); }
    [[nodiscard]] const T &Front() const                  { return m_vector.Front(); }
    [[nodiscard]] T &Back()                               { return m_vector.Back(); }
    [[nodiscard]] const T &Back() const                   { return m_vector.Back(); }

    [[nodiscard]] T &At(Iterator iter)                    { return m_vector[std::distance(m_vector.Begin(), iter)]; }
    [[nodiscard]] const T &At(Iterator iter) const        { return m_vector[std::distance(m_vector.Begin(), iter)]; }

    //[[nodiscard]] T &operator[](size_t index)             { return m_vector[index]; }
    //[[nodiscard]] const T &operator[](size_t index) const { return m_vector[index]; }

    HYP_DEF_STL_ITERATOR(m_vector)

private:
    /*Iterator LowerBound(const T &value)
    {
        return std::lower_bound(
            m_vector.begin(),
            m_vector.end(),
            value
        );
    }

    ConstIterator LowerBound(const T &value) const
    {
        return std::lower_bound(
            m_vector.cbegin(),
            m_vector.cend(),
            value
        );
    }*/
};

template <class T>
FlatSet<T>::FlatSet() {}

template <class T>
FlatSet<T>::FlatSet(const FlatSet &other)
    : m_vector(other.m_vector)
{
}

template <class T>
auto FlatSet<T>::operator=(const FlatSet &other) -> FlatSet&
{
    m_vector = other.m_vector;

    return *this;
}

template <class T>
FlatSet<T>::FlatSet(FlatSet &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class T>
auto FlatSet<T>::operator=(FlatSet &&other) noexcept -> FlatSet&
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class T>
FlatSet<T>::~FlatSet() = default;

template <class T>
auto FlatSet<T>::Find(const T &value) -> Iterator
{
    const auto it = FlatSet<T>::Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Find(const T &value) const -> ConstIterator
{
    const auto it = FlatSet<T>::Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Insert(const T &value) -> InsertResult
{
    Iterator it = FlatSet<T>::Base::LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = m_vector.Insert(it, value);

        return {it, true};
    }

    return {it, false};
}

template <class T>
auto FlatSet<T>::Insert(T &&value) -> InsertResult
{
    Iterator it = FlatSet<T>::Base::LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = m_vector.Insert(it, std::forward<T>(value));

        return {it, true};
    }

    return {it, false};
}

template <class T>
bool FlatSet<T>::Erase(Iterator it)
{
    if (it == End()) {
        return false;
    }

    m_vector.Erase(it);

    return true;
}

template <class T>
bool FlatSet<T>::Erase(const T &value)
{
    return Erase(Find(value));
}

} // namespace hyperion

#endif
