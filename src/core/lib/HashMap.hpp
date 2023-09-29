#ifndef HYPERION_V2_LIB_HASH_MAP_HPP
#define HYPERION_V2_LIB_HASH_MAP_HPP

#include <HashCode.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/ContainerBase.hpp>

namespace hyperion {

template <class KeyType, class ValueType>
struct HashElement
{
    HashCode::ValueType hash_code;

    KeyType key;
    ValueType value;
};

template <class KeyType, class ValueType>
struct HashBucket
{
    using ElementList = Array<HashElement<KeyType, ValueType>>;

    struct ConstIterator;

    struct Iterator
    {
        HashBucket<KeyType, ValueType> *bucket;
        SizeType index;

        HashElement<KeyType, ValueType> *operator->() const
            { return &bucket->elements[index]; }

        HashElement<KeyType, ValueType> &operator*()
            { return bucket->elements[index]; }

        const HashElement<KeyType, ValueType> &operator*() const
            { return bucket->elements[index]; }

        Iterator &operator++()
            { ++index; return *this; }

        Iterator operator++(int) const
            { return Iterator { bucket, index + 1 }; }

        bool operator==(const ConstIterator &other) const
            { return bucket == other.bucket && index == other.index; }

        bool operator!=(const ConstIterator &other) const
            { return bucket != other.bucket || index != other.index; }

        bool operator==(const Iterator &other) const
            { return bucket == other.bucket && index == other.index; }

        bool operator!=(const Iterator &other) const
            { return bucket != other.bucket || index != other.index; }

        operator ConstIterator() const
            { return { bucket, index }; }
    };

    struct ConstIterator
    {
        const HashBucket<KeyType, ValueType> *bucket;
        SizeType index;

        const HashElement<KeyType, ValueType> *operator->() const
            { return &bucket->elements[index]; }

        const HashElement<KeyType, ValueType> &operator*() const
            { return bucket->elements[index]; }

        ConstIterator &operator++()
            { ++index; return *this; }

        ConstIterator operator++(int) const
            { return ConstIterator { bucket, index + 1 }; }

        bool operator==(const ConstIterator &other) const
            { return bucket == other.bucket && index == other.index; }

        bool operator!=(const ConstIterator &other) const
            { return bucket != other.bucket || index != other.index; }

        bool operator==(const Iterator &other) const
            { return bucket == other.bucket && index == other.index; }

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

template <class KeyType, class ValueType>
class HashMap : public ContainerBase<HashMap<KeyType, ValueType>, KeyType>
{
    static constexpr SizeType initial_bucket_size = 16;
    static constexpr Double load_factor = 0.75;

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

    struct ConstIterator;

    struct Iterator
    {
        HashMap *hm;
        typename HashBucket<KeyType, ValueType>::Iterator bucket_iter;

        Iterator(HashMap *hm, typename HashBucket<KeyType, ValueType>::Iterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        Iterator(const Iterator &other) = default;
        Iterator &operator=(const Iterator &other) = default;
        Iterator(Iterator &&other) noexcept = default;
        Iterator &operator=(Iterator &other) &noexcept = default;
        ~Iterator() = default;

        HashElement<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }

        HashElement<KeyType, ValueType> &operator*()
            {  return bucket_iter.operator*(); }

        const HashElement<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }

        Iterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }

        Iterator operator++(int) const
        {
            Iterator iter(*this);
            AdvanceIterator(iter);

            return iter;
        }

        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }

        operator ConstIterator() const
            { return ConstIterator { const_cast<const HashMap *>(hm), typename HashBucket<KeyType, ValueType>::ConstIterator(bucket_iter) }; }
    };

    struct ConstIterator
    {
        const HashMap *hm;
        typename HashBucket<KeyType, ValueType>::ConstIterator bucket_iter;

        ConstIterator(const HashMap *hm, typename HashBucket<KeyType, ValueType>::ConstIterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        ConstIterator(const ConstIterator &other) = default;
        ConstIterator &operator=(const ConstIterator &other) = default;
        ConstIterator(ConstIterator &&other) noexcept = default;
        ConstIterator &operator=(ConstIterator &other) &noexcept = default;
        ~ConstIterator() = default;

        const HashElement<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }

        const HashElement<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }

        ConstIterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }

        ConstIterator operator++(int) const
        {
            ConstIterator iter = *this;
            AdvanceIterator(iter);

            return iter;
        }

        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }
    };

    using InsertResult = Pair<Iterator, Bool>;

    HashMap();
    HashMap(const HashMap &other);
    HashMap &operator=(const HashMap &other);
    HashMap(HashMap &&other) noexcept;
    HashMap &operator=(HashMap &&other) noexcept;
    ~HashMap() = default;

    ValueType &operator[](const KeyType &key);

#ifndef HYP_DEBUG_MODE
    [[nodiscard]] ValueType &At(const KeyType &key)             { return Find(key)->value; }
    [[nodiscard]] const ValueType &At(const KeyType &key) const { return Find(key)->value; }
#else
    [[nodiscard]] ValueType &At(const KeyType &key)
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->value;
    }

    [[nodiscard]] const ValueType &At(const KeyType &key) const
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->value;
    }
#endif

    [[nodiscard]] bool Any() const { return m_buckets.Any([](const HashBucket<KeyType, ValueType> &bucket) { return bucket.elements.Any(); }); }
    [[nodiscard]] bool Empty() const { return !Any(); }

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

    void Clear();

    Iterator Begin() { return Iterator(this, typename HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data(), SizeType(0) }); }
    Iterator End() { return Iterator(this, typename HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data() + m_buckets.Size(), SizeType(0) }); }
    ConstIterator Begin() const { return ConstIterator(this, typename HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data(), SizeType(0) }); }
    ConstIterator End() const { return ConstIterator(this, typename HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data() + m_buckets.Size(), SizeType(0) }); }

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator begin() const { return Begin(); }
    ConstIterator end() const { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }

private:
    void CheckAndRebuildBuckets();

    HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash);
    const HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash) const;

    void SetElement(HashElement<KeyType, ValueType> &&element);
    InsertResult InsertElement(HashElement<KeyType, ValueType> &&element);

    Array<HashBucket<KeyType, ValueType>, initial_bucket_size * sizeof(HashBucket<KeyType, ValueType>)> m_buckets;
};

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap()
{
    m_buckets.Resize(initial_bucket_size);
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(const HashMap &other)
    : m_buckets(other.m_buckets)
{
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(const HashMap &other) -> HashMap &
{
    m_buckets = other.m_buckets;

    return *this;
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(HashMap &&other) noexcept
    : m_buckets(std::move(other.m_buckets))
{
    other.m_buckets.Resize(initial_bucket_size);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(HashMap &&other) noexcept -> HashMap &
{
    m_buckets = std::move(other.m_buckets);
    other.m_buckets.Resize(initial_bucket_size);

    return *this;
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator[](const KeyType &key) -> ValueType &
{
    const Iterator it = Find(key);

    if (it != End()) {
        return it->value;
    }

    return Insert(key, ValueType { }).first->value;
}

template <class KeyType, class ValueType>
HashBucket<KeyType, ValueType> *HashMap<KeyType, ValueType>::GetBucketForHash(HashCode::ValueType hash)
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(m_buckets.Size() != 0);
#endif

    return &m_buckets[hash % m_buckets.Size()];
}

template <class KeyType, class ValueType>
const HashBucket<KeyType, ValueType> *HashMap<KeyType, ValueType>::GetBucketForHash(HashCode::ValueType hash) const
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(m_buckets.Size() != 0);
#endif

    return &m_buckets[hash % m_buckets.Size()];
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::CheckAndRebuildBuckets()
{
#if 0 // TODO
    SizeType total_elements = 0;

    for (const auto &bucket : m_buckets) {
        total_elements += bucket.Size();
    }

    const Double current_load_factor = Double(total_elements) / Double(m_buckets.Size());

    // TODO: Calculate # of buckets such that load factor is not exceeded

    if (current_load_factor > load_factor) {
        const Double load_factor_div = current_load_factor / load_factor;


        Array<HashBucket<KeyType, ValueType>> new_buckets;
    }
#endif
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) -> Iterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

   const typename HashBucket<KeyType, ValueType>::Iterator it = bucket->Find(hash_code);

    if (it == bucket->End()) {
        return End();
    }

    return Iterator(this, it);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) const -> ConstIterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    const HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    const typename HashBucket<KeyType, ValueType>::ConstIterator it = bucket->Find(hash_code);

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

#ifdef HYP_DEBUG_MODE
    AssertThrow(iter.hm == this);
    AssertThrow(iter.bucket_iter.bucket != nullptr);
    AssertThrow(iter.bucket_iter.index < iter.bucket_iter.bucket->elements.Size());
#endif

    const typename HashBucket<KeyType, ValueType>::ElementList::Iterator element_list_it { iter.bucket_iter.bucket->elements.Data() + iter.bucket_iter.index };

    const auto erase_it = iter.bucket_iter.bucket->elements.Erase(element_list_it);
    const SizeType erase_index = std::distance(iter.bucket_iter.bucket->elements.Begin(), erase_it);

    return Iterator(this, typename HashBucket<KeyType, ValueType>::Iterator { iter.bucket_iter.bucket, erase_index });
}

template <class KeyType, class ValueType>
bool HashMap<KeyType, ValueType>::Erase(const KeyType &key)
{
    const Iterator it = Find(key);

    if (it == End()) {
        return false;
    }

#ifdef HYP_DEBUG_MODE
    AssertThrow(it.hm == this);
    AssertThrow(it.bucket_iter.bucket != nullptr);
    AssertThrow(it.bucket_iter.index < it.bucket_iter.bucket->elements.Size());
#endif

    const typename HashBucket<KeyType, ValueType>::ElementList::ConstIterator element_list_it { it.bucket_iter.bucket->elements.Data() + it.bucket_iter.index };

    const auto erase_it = it.bucket_iter.bucket->elements.Erase(element_list_it);

    return erase_it != it.bucket_iter.bucket->elements.End();
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::SetElement(HashElement<KeyType, ValueType> &&element)
{
    HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(element.hash_code);

    auto bucket_it = bucket->Find(element.hash_code);

    if (bucket_it != bucket->End()) {
        *bucket_it = std::move(element);
    } else {
        bucket->Push(std::move(element));
    }
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::InsertElement(HashElement<KeyType, ValueType> &&element) -> InsertResult
{
    CheckAndRebuildBuckets();

    HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(element.hash_code);

    auto bucket_it = bucket->Find(element.hash_code);

    Iterator insert_it(this, bucket_it);

    if (bucket_it != bucket->End()) {
        return InsertResult { insert_it, false };
    }

    insert_it.bucket_iter = bucket->Push(std::move(element));

    return InsertResult { insert_it, true };
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, const ValueType &value)
{
    SetElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, ValueType &&value)
{
    SetElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, const ValueType &value)
{
    SetElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, ValueType &&value)
{
    SetElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, const ValueType &value) -> InsertResult
{
    return InsertElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, ValueType &&value) -> InsertResult
{
    return InsertElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, const ValueType &value) -> InsertResult
{
    return InsertElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, ValueType &&value) -> InsertResult
{
    return InsertElement(HashElement<KeyType, ValueType> {
        HashCode::GetHashCode(key).Value(),
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Clear()
{
    m_buckets.Clear();
    m_buckets.Resize(initial_bucket_size);
}

} // namespace hyperion

#endif
