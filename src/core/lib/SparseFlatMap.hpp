#ifndef HYPERION_V2_LIB_SPARSE_FLAT_MAP_H
#define HYPERION_V2_LIB_SPARSE_FLAT_MAP_H

#include "FlatSet.hpp"
#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Key, class Value>
class SparseMap {
    std::vector<Value> m_vector;
    std::vector<std::pair<Key, Value>> m_tmp_vector;

public:
    using Iterator      = typename std::vector<Value>::iterator;
    using ConstIterator = typename std::vector<Value>::const_iterator;
    using InsertResult  = std::pair<Iterator, bool>; // iterator, was inserted

    SparseMap();

    SparseMap(std::initializer_list<std::pair<Key, Value>> values)
        : m_tmp_vector(values)
    {
        if (!m_tmp_vector.empty()) {
            std::sort(
                m_tmp_vector.begin(),
                m_tmp_vector.end(),
                [](const auto &lhs, const auto &rhs) {
                    return lhs.first < rhs.first;
                }
            );

            m_vector.resize(m_tmp_vector.back().first + 1);

            for (auto &it : m_tmp_vector) {
                m_vector[it.first] = std::move(it.second);
            }
        }

        m_tmp_vector.clear();
    }

    SparseMap(const SparseMap &other);
    SparseMap &operator=(const SparseMap &other);
    SparseMap(SparseMap &&other) noexcept;
    SparseMap &operator=(SparseMap &&other) noexcept;
    ~SparseMap();
    
    [[nodiscard]] Iterator Find(const Key &key);
    [[nodiscard]] ConstIterator Find(const Key &key) const;

    [[nodiscard]] bool Contains(const Key &key) const;
    
    Value &operator[](const Key &key);

    [[nodiscard]] size_t Size() const                    { return m_vector.size(); }
    [[nodiscard]] Value *Data()                          { return m_vector.data(); }
    [[nodiscard]] Value * const Data() const             { return m_vector.data(); }
    [[nodiscard]] bool Empty() const                     { return m_vector.empty(); }

    void Clear()                                         { m_vector.clear(); }
    
    [[nodiscard]] Value &Front()                         { return m_vector.front(); }
    [[nodiscard]] const Value &Front() const             { return m_vector.front(); }
    [[nodiscard]] Value &Back()                          { return m_vector.back(); }
    [[nodiscard]] const Value &Back() const              { return m_vector.back(); }

    [[nodiscard]] Value &At(const Key &key);
    [[nodiscard]] const Value &At(const Key &key) const;

    HYP_DEF_STL_ITERATOR(m_vector)
};

template <class Key, class Value>
SparseMap<Key, Value>::SparseMap() {}

template <class Key, class Value>
SparseMap<Key, Value>::SparseMap(const SparseMap &other)
    : m_vector(other.m_vector)
{
}

template <class Key, class Value>
auto SparseMap<Key, Value>::operator=(const SparseMap &other) -> SparseMap&
{
    m_vector = other.m_vector;

    return *this;
}

template <class Key, class Value>
SparseMap<Key, Value>::SparseMap(SparseMap &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class Key, class Value>
auto SparseMap<Key, Value>::operator=(SparseMap &&other) noexcept -> SparseMap&
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class Key, class Value>
SparseMap<Key, Value>::~SparseMap() = default;

template <class Key, class Value>
auto SparseMap<Key, Value>::Find(const Key &key) -> Iterator
{
    if (Size() >= key) {
        return End();
    }

    return m_vector.begin() + key;
}

template <class Key, class Value>
auto SparseMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    if (Size() >= key) {
        return End();
    }

    return m_vector.begin() + key;
}

template <class Key, class Value>
bool SparseMap<Key, Value>::Contains(const Key &key) const
{
    return Size() < key;
}

template <class Key, class Value>
auto SparseMap<Key, Value>::operator[](const Key &key) -> Value&
{
    if (Size() <= key) {
        m_vector.resize(key + 1);
    }

    return m_vector[key];
}

template <class Key, class Value>
auto SparseMap<Key, Value>::At(const Key &key) -> Value&
{
    AssertThrow(key < Size());

    return m_vector[key];
}

template <class Key, class Value>
auto SparseMap<Key, Value>::At(const Key &key) const -> const Value&
{
    AssertThrow(key < Size());

    return m_vector[key];
}

} // namespace hyperion

#endif
