#ifndef HYPERION_V2_LIB_FLAT_SET_H
#define HYPERION_V2_LIB_FLAT_SET_H

#include <util/Defines.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class T>
class FlatSet {
    std::vector<T> m_vector;

public:
    using Iterator      = typename std::vector<T>::iterator;
    using ConstIterator = typename std::vector<T>::const_iterator;
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

    [[nodiscard]] size_t Size() const                     { return m_vector.size(); }
    [[nodiscard]] T *Data()                               { return m_vector.data(); }
    [[nodiscard]] T * const Data() const                  { return m_vector.data(); }
    [[nodiscard]] bool Empty() const                      { return m_vector.empty(); }
    [[nodiscard]] bool Contains(const T &value) const     { return Find(value) != End(); }
    void Clear()                                          { m_vector.clear(); }
    
    [[nodiscard]] T &Front()                              { return m_vector.front(); }
    [[nodiscard]] const T &Front() const                  { return m_vector.front(); }
    [[nodiscard]] T &Back()                               { return m_vector.back(); }
    [[nodiscard]] const T &Back() const                   { return m_vector.back(); }

    [[nodiscard]] T &At(Iterator iter)                    { return m_vector.at(std::distance(m_vector.begin(), iter)); }
    [[nodiscard]] const T &At(Iterator iter) const        { return m_vector.at(std::distance(m_vector.begin(), iter)); }

    //[[nodiscard]] T &operator[](size_t index)             { return m_vector[index]; }
    //[[nodiscard]] const T &operator[](size_t index) const { return m_vector[index]; }

    HYP_DEF_STL_ITERATOR(m_vector)

private:
    Iterator LowerBound(const T &value)
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
    }
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
    const auto it = LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Find(const T &value) const -> ConstIterator
{
    const auto it = LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto FlatSet<T>::Insert(const T &value) -> InsertResult
{
    Iterator it = LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = m_vector.insert(it, value);

        return {it, true};
    }

    return {it, false};
}

template <class T>
auto FlatSet<T>::Insert(T &&value) -> InsertResult
{
    Iterator it = LowerBound(value);

    if (it == End() || !(*it == value)) {
        it = m_vector.insert(it, std::forward<T>(value));

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

    m_vector.erase(it);

    return true;
}

template <class T>
bool FlatSet<T>::Erase(const T &value)
{
    return Erase(Find(value));
}

} // namespace hyperion

#endif
