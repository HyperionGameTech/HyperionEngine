/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HASH_MAP_HPP
#define HYPERION_HASH_MAP_HPP

#include <HashCode.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/ContainerBase.hpp>

namespace hyperion {

namespace containers {
namespace detail {

template <class KeyType, class ValueType>
struct HashElement
{
    HashCode::ValueType hash_code;

    KeyType     first;
    ValueType   second;

    /*! \brief Implement GetHashCode() so GetHashCode() of the entire HashMap can be used. */
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class KeyType, class ValueType>
struct HashBucket
{
    using ElementList = Array<HashElement<KeyType, ValueType>>;

    struct ConstIterator;

    struct Iterator
    {
        HashBucket<KeyType, ValueType>  *bucket;
        SizeType                        index;

        HYP_FORCE_INLINE
        HashElement<KeyType, ValueType> *operator->() const
            { return &bucket->elements[index]; }

        HYP_FORCE_INLINE
        HashElement<KeyType, ValueType> &operator*()
            { return bucket->elements[index]; }

        HYP_FORCE_INLINE
        const HashElement<KeyType, ValueType> &operator*() const
            { return bucket->elements[index]; }

        HYP_FORCE_INLINE
        Iterator &operator++()
            { ++index; return *this; }

        HYP_FORCE_INLINE
        Iterator operator++(int) const
            { return Iterator { bucket, index + 1 }; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket == other.bucket && index == other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket != other.bucket || index != other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket == other.bucket && index == other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket != other.bucket || index != other.index; }

        HYP_FORCE_INLINE
        operator ConstIterator() const
            { return { bucket, index }; }
    };

    struct ConstIterator
    {
        const HashBucket<KeyType, ValueType> *bucket;
        SizeType index;

        HYP_FORCE_INLINE
        const HashElement<KeyType, ValueType> *operator->() const
            { return &bucket->elements[index]; }

        HYP_FORCE_INLINE
        const HashElement<KeyType, ValueType> &operator*() const
            { return bucket->elements[index]; }

        HYP_FORCE_INLINE
        ConstIterator &operator++()
            { ++index; return *this; }

        HYP_FORCE_INLINE
        ConstIterator operator++(int) const
            { return ConstIterator { bucket, index + 1 }; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket == other.bucket && index == other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket != other.bucket || index != other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket == other.bucket && index == other.index; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket != other.bucket || index != other.index; }
    };
    
    ElementList elements;
    
    Iterator Push(HashElement<KeyType, ValueType> &&element)
    {
        const Iterator iter { this, elements.Size() };

        elements.PushBack(std::move(element));

        return iter;
    }

    Iterator Find(HashCode::ValueType hash)
    {
        for (auto it = elements.Begin(); it != elements.End(); ++it) {
            if (it->hash_code == hash) {
                return Iterator { this, SizeType(std::distance(elements.Begin(), it)) };
            }
        }

        return End();
    }

    ConstIterator Find(HashCode::ValueType hash) const
    {
        for (auto it = elements.Begin(); it != elements.End(); ++it) {
            if (it->hash_code == hash) {
                return ConstIterator { this, SizeType(std::distance(elements.Begin(), it)) };
            }
        }

        return End();
    }

    Iterator Begin() { return { this, 0 }; }
    Iterator End() { return { this, elements.Size() }; }
    ConstIterator Begin() const { return { this, 0 }; }
    ConstIterator End() const { return { this, elements.Size() }; }

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator begin() const { return Begin(); }
    ConstIterator end() const { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }
};

} // namespace detail

template <class KeyType, class ValueType>
class HashMap : public ContainerBase<HashMap<KeyType, ValueType>, KeyType>
{
    static constexpr bool is_contiguous = false;

    static constexpr SizeType initial_bucket_size = 16;
    static constexpr double desired_load_factor = 0.9;

    template <class IteratorType>
    static inline void AdvanceIteratorBucket(IteratorType &iter)
    {
        while (iter.bucket_iter.bucket != iter.hm->m_buckets.End() && iter.bucket_iter.index >= iter.bucket_iter.bucket->elements.Size()) {
            ++iter.bucket_iter.bucket;
            iter.bucket_iter.index = 0;
        }
    }

    template <class IteratorType>
    static inline void AdvanceIterator(IteratorType &iter)
    {
        ++iter.bucket_iter.index;

        AdvanceIteratorBucket(iter);
    }

public:
    using Base = ContainerBase<HashMap<KeyType, ValueType>, KeyType>;
    using KeyValuePairType = KeyValuePair<KeyType, ValueType>;

    struct ConstIterator;

    struct Iterator
    {
        HashMap *hm;
        typename detail::HashBucket<KeyType, ValueType>::Iterator bucket_iter;

        Iterator(HashMap *hm, typename detail::HashBucket<KeyType, ValueType>::Iterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        Iterator(const Iterator &other)                 = default;
        Iterator &operator=(const Iterator &other)      = default;
        Iterator(Iterator &&other) noexcept             = default;
        Iterator &operator=(Iterator &other) &noexcept  = default;
        ~Iterator()                                     = default;
        
        HYP_FORCE_INLINE
        detail::HashElement<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE
        detail::HashElement<KeyType, ValueType> &operator*()
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        const detail::HashElement<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        Iterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE
        Iterator operator++(int) const
        {
            Iterator iter(*this);
            AdvanceIterator(iter);

            return iter;
        }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }

        operator ConstIterator() const
            { return ConstIterator { const_cast<const HashMap *>(hm), typename detail::HashBucket<KeyType, ValueType>::ConstIterator(bucket_iter) }; }
    };

    struct ConstIterator
    {
        const HashMap *hm;
        typename detail::HashBucket<KeyType, ValueType>::ConstIterator bucket_iter;

        ConstIterator(const HashMap *hm, typename detail::HashBucket<KeyType, ValueType>::ConstIterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        ConstIterator(const ConstIterator &other)                   = default;
        ConstIterator &operator=(const ConstIterator &other)        = default;
        ConstIterator(ConstIterator &&other) noexcept               = default;
        ConstIterator &operator=(ConstIterator &other) &noexcept    = default;
        ~ConstIterator()                                            = default;
        
        HYP_FORCE_INLINE
        const detail::HashElement<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE
        const detail::HashElement<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        ConstIterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE
        ConstIterator operator++(int) const
        {
            ConstIterator iter = *this;
            AdvanceIterator(iter);

            return iter;
        }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }
    };

    using InsertResult = Pair<Iterator, bool>;

    HashMap();

    HashMap(std::initializer_list<KeyValuePairType> initializer_list)
        : HashMap()
    {
        Array<KeyValuePairType> temp(initializer_list);

        for (auto &&item : temp) {
            Set(std::move(item.first), std::move(item.second));
        }
    }

    HashMap(const HashMap &other);
    HashMap &operator=(const HashMap &other);
    HashMap(HashMap &&other) noexcept;
    HashMap &operator=(HashMap &&other) noexcept;
    ~HashMap() = default;

    ValueType &operator[](const KeyType &key);

#ifndef HYP_DEBUG_MODE
    [[nodiscard]] ValueType &At(const KeyType &key)
        { return Find(key)->second; }

    [[nodiscard]] const ValueType &At(const KeyType &key) const
        { return Find(key)->second; }
#else
    [[nodiscard]] ValueType &At(const KeyType &key)
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->second;
    }

    [[nodiscard]] const ValueType &At(const KeyType &key) const
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->second;
    }
#endif

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Any() const
        { return m_size != 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Empty() const
        { return m_size == 0; }

    [[nodiscard]] bool operator==(const HashMap &other) const
    {
        if (Size() != other.Size()) {
            return false;
        }

        for (const auto &bucket : m_buckets) {
            for (const auto &element : bucket.elements) {
                const auto other_it = other.Find(element.first);

                if (other_it == other.End()) {
                    return false;
                }

                if (other_it->second != element.second) {
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const HashMap &other) const
        { return !(*this == other); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Size() const
        { return m_size; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType BucketCount() const
        { return m_buckets.Size(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType BucketSize(SizeType bucket_index) const
        { return m_buckets[bucket_index].elements.Size(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Bucket(const KeyType &key) const
        { return HashCode::GetHashCode(key).Value() % m_buckets.Size(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    double LoadFactor() const
        { return double(Size()) / double(BucketCount()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr double MaxLoadFactor()
        { return desired_load_factor; }

    Iterator Find(const KeyType &key);
    ConstIterator Find(const KeyType &key) const;

    bool Contains(const KeyType &key) const;
    
    Iterator Erase(Iterator iter);
    bool Erase(const KeyType &key);

    void Set(const KeyType &key, const ValueType &value);
    void Set(const KeyType &key, ValueType &&value);
    void Set(KeyType &&key, const ValueType &value);
    void Set(KeyType &&key, ValueType &&value);

    InsertResult Insert(const KeyType &key, const ValueType &value);
    InsertResult Insert(const KeyType &key, ValueType &&value);
    InsertResult Insert(KeyType &&key, const ValueType &value);
    InsertResult Insert(KeyType &&key, ValueType &&value);
    InsertResult Insert(Pair<KeyType, ValueType> &&pair);

    void Clear();
    
    HYP_FORCE_INLINE
    Iterator Begin()
        { return Iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data(), SizeType(0) }); }
    
    HYP_FORCE_INLINE
    Iterator End()
        { return Iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data() + m_buckets.Size(), SizeType(0) }); }
    
    HYP_FORCE_INLINE
    ConstIterator Begin() const
        { return ConstIterator(this, typename detail::HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data(), SizeType(0) }); }
    
    HYP_FORCE_INLINE
    ConstIterator End() const
        { return ConstIterator(this, typename detail::HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data() + m_buckets.Size(), SizeType(0) }); }
    
    HYP_FORCE_INLINE
    Iterator begin()
        { return Begin(); }
    
    HYP_FORCE_INLINE
    Iterator end()
        { return End(); }
    
    HYP_FORCE_INLINE
    ConstIterator begin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE
    ConstIterator end() const
        { return End(); }
    
    HYP_FORCE_INLINE
    ConstIterator cbegin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE
    ConstIterator cend() const
        { return End(); }

private:
    void CheckAndRebuildBuckets();

    [[nodiscard]]
    HYP_FORCE_INLINE
    detail::HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash)
        { return &m_buckets[hash % m_buckets.Size()]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const detail::HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash) const
        { return &m_buckets[hash % m_buckets.Size()]; }

    void SetElement(detail::HashElement<KeyType, ValueType> &&element);
    InsertResult InsertElement(detail::HashElement<KeyType, ValueType> &&element);

    Array<detail::HashBucket<KeyType, ValueType>, initial_bucket_size * sizeof(detail::HashBucket<KeyType, ValueType>)> m_buckets;
    SizeType m_size;
};

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap()
    : m_size(0)
{
    m_buckets.Resize(initial_bucket_size);
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(const HashMap &other)
    : m_buckets(other.m_buckets),
      m_size(other.m_size)
{
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(const HashMap &other) -> HashMap &
{
    m_buckets = other.m_buckets;
    m_size = other.m_size;

    return *this;
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(HashMap &&other) noexcept
    : m_buckets(std::move(other.m_buckets)),
      m_size(other.m_size)
{
    other.m_buckets.Resize(initial_bucket_size);
    other.m_size = 0;
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(HashMap &&other) noexcept -> HashMap &
{
    m_buckets = std::move(other.m_buckets);
    other.m_buckets.Resize(initial_bucket_size);

    m_size = other.m_size;
    other.m_size = 0;

    return *this;
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator[](const KeyType &key) -> ValueType &
{
    const Iterator it = Find(key);

    if (it != End()) {
        return it->second;
    }

    return Insert(key, ValueType { }).first->second;
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::CheckAndRebuildBuckets()
{
    // Check load factor, if currently load factor is greater than `load_factor`, then rehash so that the load factor becomes <= `load_factor` constant.

    if (LoadFactor() < MaxLoadFactor()) {
        return;
    }

    const SizeType new_bucket_count = SizeType(double(BucketCount()) / MaxLoadFactor());

    Array<detail::HashBucket<KeyType, ValueType>, initial_bucket_size * sizeof(detail::HashBucket<KeyType, ValueType>)> new_buckets;
    new_buckets.Resize(new_bucket_count);

    for (auto &bucket : m_buckets) {
        for (auto &element : bucket.elements) {
            detail::HashBucket<KeyType, ValueType> *new_bucket = new_buckets.Data() + (element.hash_code % new_buckets.Size());

            new_bucket->Push(std::move(element));
        }
    }

    m_buckets = std::move(new_buckets);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) -> Iterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    const typename detail::HashBucket<KeyType, ValueType>::Iterator it = bucket->Find(hash_code);

    if (it == bucket->End()) {
        return End();
    }

    return Iterator(this, it);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) const -> ConstIterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    const detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    const typename detail::HashBucket<KeyType, ValueType>::ConstIterator it = bucket->Find(hash_code);

    if (it == bucket->End()) {
        return End();
    }

    return ConstIterator(this, it);
}

template <class KeyType, class ValueType>
bool HashMap<KeyType, ValueType>::Contains(const KeyType &key) const
{
    return Find(key) != End();
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Erase(Iterator iter) -> Iterator
{
    if (iter == End()) {
        return End();
    }

    --m_size;

    const typename detail::HashBucket<KeyType, ValueType>::ElementList::Iterator element_list_it {
        iter.bucket_iter.bucket->elements.Data() + iter.bucket_iter.index
    };

    const auto erase_it = iter.bucket_iter.bucket->elements.Erase(element_list_it);

    if (erase_it == iter.bucket_iter.bucket->elements.End()) {
        return End();
    }

    const SizeType erase_index = std::distance(iter.bucket_iter.bucket->elements.Begin(), erase_it);

    return Iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { iter.bucket_iter.bucket, erase_index });
}

template <class KeyType, class ValueType>
bool HashMap<KeyType, ValueType>::Erase(const KeyType &key)
{
    const Iterator it = Find(key);

    if (it == End()) {
        return false;
    }
    
    --m_size;

    const typename detail::HashBucket<KeyType, ValueType>::ElementList::ConstIterator element_list_it {
        it.bucket_iter.bucket->elements.Data() + it.bucket_iter.index
    };

    const auto erase_it = it.bucket_iter.bucket->elements.Erase(element_list_it);

    if (erase_it == it.bucket_iter.bucket->elements.End()) {
        return false;
    }

    return true;
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::SetElement(detail::HashElement<KeyType, ValueType> &&element)
{
    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(element.hash_code);

    auto it = bucket->Find(element.hash_code);

    if (it != bucket->End()) {
        *it = std::move(element);
    } else {
        bucket->Push(std::move(element));
        m_size++;

        CheckAndRebuildBuckets();
    }
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::InsertElement(detail::HashElement<KeyType, ValueType> &&element) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets();

    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(element.hash_code);

    auto it = bucket->Find(element.hash_code);

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        return InsertResult { insert_it, false };
    }

    insert_it.bucket_iter = bucket->Push(std::move(element));
    m_size++;

    return InsertResult { insert_it, true };
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, const ValueType &value)
{
    SetElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, ValueType &&value)
{
    SetElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, const ValueType &value)
{
    SetElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, ValueType &&value)
{
    SetElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, const ValueType &value) -> InsertResult
{
    return InsertElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, ValueType &&value) -> InsertResult
{
    return InsertElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, const ValueType &value) -> InsertResult
{
    return InsertElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, ValueType &&value) -> InsertResult
{
    return InsertElement(detail::HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(Pair<KeyType, ValueType> &&pair) -> InsertResult
{
    const HashCode hash_code = HashCode::GetHashCode(pair.first);

    return InsertElement(detail::HashElement<KeyType, ValueType> {
        hash_code.Value(),
        std::move(pair.first),
        std::move(pair.second)
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Clear()
{
    m_buckets.Clear();
    m_buckets.Resize(initial_bucket_size);

    m_size = 0;
}

} // namespace containers

using containers::HashMap;

} // namespace hyperion

#endif
