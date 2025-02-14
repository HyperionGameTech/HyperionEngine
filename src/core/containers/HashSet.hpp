/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HASH_SET_HPP
#define HYPERION_HASH_SET_HPP

#include <core/containers/Array.hpp>
#include <core/containers/ContainerBase.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace containers {
namespace detail {

template <class ValueType, auto KeyByFunction>
struct HashSetElement
{
    HashSetElement      *next = nullptr;
    HashCode::ValueType hash_code;
    ValueType           value;

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return hash_code; }
};

template <class ValueType, auto KeyByFunction>
struct HashSetBucket
{
    struct ConstIterator;

    struct Iterator
    {
        HashSetBucket<ValueType, KeyByFunction>     *bucket;
        HashSetElement<ValueType, KeyByFunction>    *element;

        HYP_FORCE_INLINE ValueType *operator->() const
            { return &element->value; }

        HYP_FORCE_INLINE ValueType &operator*() const
            { return element->value; }

        HYP_FORCE_INLINE Iterator &operator++()
            { element = element->next; return *this; }

        HYP_FORCE_INLINE Iterator operator++(int) const
            { return Iterator { bucket, element->next }; }

        HYP_FORCE_INLINE bool operator==(const ConstIterator &other) const
            { return element == other.element; }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator &other) const
            { return element != other.element; }

        HYP_FORCE_INLINE bool operator==(const Iterator &other) const
            { return element == other.element; }

        HYP_FORCE_INLINE bool operator!=(const Iterator &other) const
            { return element != other.element; }

        HYP_FORCE_INLINE operator ConstIterator() const
            { return { bucket, element }; }
    };

    struct ConstIterator
    {
        const HashSetBucket<ValueType, KeyByFunction>   *bucket;
        const HashSetElement<ValueType, KeyByFunction>  *element;

        HYP_FORCE_INLINE const ValueType *operator->() const
            { return &element->value; }

        HYP_FORCE_INLINE const ValueType &operator*() const
            { return element->value; }

        HYP_FORCE_INLINE ConstIterator &operator++()
            { element = element->next; return *this; }

        HYP_FORCE_INLINE ConstIterator operator++(int) const
            { return ConstIterator { bucket, element->next }; }

        HYP_FORCE_INLINE bool operator==(const ConstIterator &other) const
            { return element == other.element; }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator &other) const
            { return element != other.element; }

        HYP_FORCE_INLINE bool operator==(const Iterator &other) const
            { return element == other.element; }

        HYP_FORCE_INLINE bool operator!=(const Iterator &other) const
            { return element != other.element; }
    };
    
    HashSetElement<ValueType, KeyByFunction>   *head = nullptr;
    HashSetElement<ValueType, KeyByFunction>   *tail = nullptr;
    
    Iterator Push(HashSetElement<ValueType, KeyByFunction> *element)
    {
        if (head == nullptr) {
            head = element;
            tail = head;
        } else {
            tail->next = element;
            tail = tail->next;
        }

        tail->next = nullptr;

        return { this, tail };
    }

    template <class TFindAsType>
    Iterator Find(const TFindAsType &value)
    {
        for (auto it = head; it != nullptr; it = it->next) {
            if (KeyByFunctionInvoker<KeyByFunction>()(it->value) == value) {
                return Iterator { this, it };
            }
        }

        return End();
    }

    template <class TFindAsType>
    ConstIterator Find(const TFindAsType &value) const
    {
       for (auto it = head; it != nullptr; it = it->next) {
            if (KeyByFunctionInvoker<KeyByFunction>()(it->value) == value) {
                return ConstIterator { this, it };
            }
        }

        return End();
    }

    Iterator Begin() { return { this, head }; }
    Iterator End() { return { this, nullptr }; }
    ConstIterator Begin() const { return { this, head }; }
    ConstIterator End() const { return { this, nullptr }; }

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator begin() const { return Begin(); }
    ConstIterator end() const { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }
};

} // namespace detail

template <class ValueType, auto KeyByFunction = &KeyBy_Identity<ValueType>>
class HashSet : public ContainerBase<HashSet<ValueType, KeyByFunction>, decltype(std::declval<KeyByFunctionInvoker<KeyByFunction>>()(std::declval<ValueType>()))>
{
    static constexpr bool is_contiguous = false;

    static constexpr SizeType initial_bucket_size = 16;
    static constexpr double desired_load_factor = 0.75;

    template <class IteratorType>
    static inline void AdvanceIteratorBucket(IteratorType &iter)
    {
        const auto *end = iter.hash_set->m_buckets.End();

        while (iter.bucket_iter.element == nullptr && iter.bucket_iter.bucket != end) {
            if (++iter.bucket_iter.bucket == end) {
                break;
            }

            iter.bucket_iter.element = iter.bucket_iter.bucket->head;
        }
    }

    template <class IteratorType>
    static inline void AdvanceIterator(IteratorType &iter)
    {
        iter.bucket_iter.element = iter.bucket_iter.element->next;

        AdvanceIteratorBucket(iter);
    }

public:
    using KeyByType = decltype(std::declval<KeyByFunctionInvoker<KeyByFunction>>()(std::declval<ValueType>()));
    using Base = ContainerBase<HashSet<ValueType, KeyByFunction>, KeyByType>;

    struct ConstIterator;

    struct Iterator
    {
        HashSet                                                             *hash_set;
        typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator  bucket_iter;

        Iterator(HashSet *hash_set, typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator bucket_iter)
            : hash_set(hash_set),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        Iterator(const Iterator &other)                 = default;
        Iterator &operator=(const Iterator &other)      = default;
        Iterator(Iterator &&other) noexcept             = default;
        Iterator &operator=(Iterator &other) &noexcept  = default;
        ~Iterator()                                     = default;
        
        HYP_FORCE_INLINE ValueType *operator->() const
            { return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE ValueType &operator*() const
            { return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE Iterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE Iterator operator++(int) const
        {
            Iterator iter(*this);
            AdvanceIterator(iter);

            return iter;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        HYP_FORCE_INLINE bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        HYP_FORCE_INLINE bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }

        HYP_FORCE_INLINE operator ConstIterator() const
            { return ConstIterator { const_cast<const HashSet *>(hash_set), typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator(bucket_iter) }; }
    };

    struct ConstIterator
    {
        const HashSet                                                           *hash_set;
        typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator bucket_iter;

        ConstIterator(const HashSet *hash_set, typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator bucket_iter)
            : hash_set(hash_set),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        ConstIterator(const ConstIterator &other)                   = default;
        ConstIterator &operator=(const ConstIterator &other)        = default;
        ConstIterator(ConstIterator &&other) noexcept               = default;
        ConstIterator &operator=(ConstIterator &other) &noexcept    = default;
        ~ConstIterator()                                            = default;
        
        HYP_FORCE_INLINE const ValueType *operator->() const
            { return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE const ValueType &operator*() const
            { return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE ConstIterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE ConstIterator operator++(int) const
        {
            ConstIterator iter = *this;
            AdvanceIterator(iter);

            return iter;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        HYP_FORCE_INLINE bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        HYP_FORCE_INLINE bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }
    };

    using InsertResult = Pair<Iterator, bool>;

    HashSet();

    HashSet(std::initializer_list<ValueType> initializer_list)
        : HashSet()
    {
        Array<ValueType> temp(initializer_list);

        for (auto &&item : temp) {
            Set(std::move(item));
        }
    }

    HashSet(const HashSet &other);
    HashSet &operator=(const HashSet &other);
    HashSet(HashSet &&other) noexcept;
    HashSet &operator=(HashSet &&other) noexcept;
    ~HashSet();

    HYP_FORCE_INLINE bool Any() const
        { return m_size != 0; }

    HYP_FORCE_INLINE bool Empty() const
        { return m_size == 0; }

    HYP_FORCE_INLINE bool operator==(const HashSet &other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const HashSet &other) const = delete;

    HYP_FORCE_INLINE SizeType Size() const
        { return m_size; }
 
    HYP_FORCE_INLINE SizeType BucketCount() const
        { return m_buckets.Size(); }
    
    HYP_FORCE_INLINE double LoadFactor() const
        { return double(Size()) / double(BucketCount()); }

    HYP_FORCE_INLINE static constexpr double MaxLoadFactor()
        { return desired_load_factor; }

    void Reserve(SizeType size);

    Iterator Find(const KeyByType &value);
    ConstIterator Find(const KeyByType &value) const;

    template <class TFindAsType>
    Iterator FindAs(const TFindAsType &value)
    {
        const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
        detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

        typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

        if (it == bucket->End()) {
            return End();
        }

        return Iterator(this, it);
    }

    template <class TFindAsType>
    ConstIterator FindAs(const TFindAsType &value) const
    {
        const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
        const detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

        const typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

        if (it == bucket->End()) {
            return End();
        }

        return ConstIterator(this, it);
    }

    HYP_FORCE_INLINE bool Contains(const KeyByType &value) const
        { return Find(value) != End(); }

    HYP_FORCE_INLINE ValueType &At(const KeyByType &value)
    {
        auto it = Find(value);
        AssertDebugMsg(it != End(), "At(): element not found");

        return *it;
    }

    HYP_FORCE_INLINE const ValueType &At(const KeyByType &value) const
    {
        auto it = Find(value);
        AssertDebugMsg(it != End(), "At(): element not found");

        return *it;
    }
    
    Iterator Erase(ConstIterator iter);
    bool Erase(const KeyByType &value);

    InsertResult Set(const ValueType &value);
    InsertResult Set(ValueType &&value);

    InsertResult Insert(const ValueType &value);
    InsertResult Insert(ValueType &&value);

    void Clear();

    template <class OtherContainerType>
    HashSet &Merge(const OtherContainerType &other)
    {
        for (const auto &item : other) {
            Insert(item);
        }

        return *this;
    }

    template <class OtherContainerType>
    HashSet &Merge(OtherContainerType &&other)
    {
        for (auto &item : other) {
            Insert(std::move(item));
        }

        other.Clear();

        return *this;
    }
    
    HYP_FORCE_INLINE Iterator Begin()
        { return Iterator(this, typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator { m_buckets.Data(), m_buckets[0].head }); }
    
    HYP_FORCE_INLINE Iterator End()
        { return Iterator(this, typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator { m_buckets.Data() + m_buckets.Size(), nullptr }); }
    
    HYP_FORCE_INLINE ConstIterator Begin() const
        { return ConstIterator(this, typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator { m_buckets.Data(), m_buckets[0].head }); }
    
    HYP_FORCE_INLINE ConstIterator End() const
        { return ConstIterator(this, typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator { m_buckets.Data() + m_buckets.Size(), nullptr }); }
    
    HYP_FORCE_INLINE Iterator begin()
        { return Begin(); }
    
    HYP_FORCE_INLINE Iterator end()
        { return End(); }
    
    HYP_FORCE_INLINE ConstIterator begin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE ConstIterator end() const
        { return End(); }
    
    HYP_FORCE_INLINE ConstIterator cbegin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE ConstIterator cend() const
        { return End(); }

private:
    HYP_FORCE_INLINE static HashCode GetHashCode(const ValueType &value)
        { return HashCode::GetHashCode(KeyByFunctionInvoker<KeyByFunction>()(value)); }

    void CheckAndRebuildBuckets();

    HYP_FORCE_INLINE detail::HashSetBucket<ValueType, KeyByFunction> *GetBucketForHash(HashCode::ValueType hash)
        { return &m_buckets[hash % m_buckets.Size()]; }

    HYP_FORCE_INLINE const detail::HashSetBucket<ValueType, KeyByFunction> *GetBucketForHash(HashCode::ValueType hash) const
        { return &m_buckets[hash % m_buckets.Size()]; }

    Array<detail::HashSetBucket<ValueType, KeyByFunction>, initial_bucket_size * sizeof(detail::HashSetBucket<ValueType, KeyByFunction>)> m_buckets;
    SizeType m_size;
};

template <class ValueType, auto KeyByFunction>
HashSet<ValueType, KeyByFunction>::HashSet()
    : m_size(0)
{
    m_buckets.Resize(initial_bucket_size);
}

template <class ValueType, auto KeyByFunction>
HashSet<ValueType, KeyByFunction>::HashSet(const HashSet &other)
    : m_size(other.m_size)
{
    m_buckets.Resize(other.m_buckets.Size());

    for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++) {
        const auto &bucket = other.m_buckets[bucket_index];

        for (auto it = bucket.head; it != nullptr; it = it->next) {
            m_buckets[bucket_index].Push(new detail::HashSetElement<ValueType, KeyByFunction> {
                nullptr,
                it->hash_code,
                it->value
            });
        }
    }
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::operator=(const HashSet &other) -> HashSet &
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }
    
    m_size = other.m_size;

    m_buckets.Clear();
    m_buckets.Resize(other.m_buckets.Size());

    for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++) {
        const auto &bucket = other.m_buckets[bucket_index];

        for (auto it = bucket.head; it != nullptr; it = it->next) {
            m_buckets[bucket_index].Push(new detail::HashSetElement<ValueType, KeyByFunction> {
                nullptr,
                it->hash_code,
                it->value
            });
        }
    }

    return *this;
}

template <class ValueType, auto KeyByFunction>
HashSet<ValueType, KeyByFunction>::HashSet(HashSet &&other) noexcept
    : m_buckets(std::move(other.m_buckets)),
      m_size(other.m_size)
{
    other.m_buckets.Resize(initial_bucket_size);
    other.m_size = 0;
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::operator=(HashSet &&other) noexcept -> HashSet &
{
    m_buckets = std::move(other.m_buckets);
    other.m_buckets.Resize(initial_bucket_size);

    m_size = other.m_size;
    other.m_size = 0;

    return *this;
}

template <class ValueType, auto KeyByFunction>
HashSet<ValueType, KeyByFunction>::~HashSet()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }
}

template <class ValueType, auto KeyByFunction>
void HashSet<ValueType, KeyByFunction>::Reserve(SizeType size)
{
    const SizeType new_bucket_count = SizeType(MathUtil::Ceil(double(size) / MaxLoadFactor()));

    if (new_bucket_count <= m_buckets.Size()) {
        return;
    }

    Array<detail::HashSetBucket<ValueType, KeyByFunction>, initial_bucket_size * sizeof(detail::HashSetBucket<ValueType, KeyByFunction>)> new_buckets;
    new_buckets.Resize(new_bucket_count);

    for (auto &bucket : m_buckets) {
        for (auto it = bucket.head; it != nullptr;) {
            detail::HashSetBucket<ValueType, KeyByFunction> *new_bucket = new_buckets.Data() + (it->hash_code % new_buckets.Size());

            detail::HashSetElement<ValueType, KeyByFunction> *next = it->next;
            it->next = nullptr;

            new_bucket->Push(it);

            it = next;
        }
    }

    m_buckets = std::move(new_buckets);
}

template <class ValueType, auto KeyByFunction>
void HashSet<ValueType, KeyByFunction>::CheckAndRebuildBuckets()
{
    // Check load factor, if currently load factor is greater than `load_factor`, then rehash so that the load factor becomes <= `load_factor` constant.

    if (LoadFactor() < MaxLoadFactor()) {
        return;
    }

    Reserve(Size() * 2);
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Find(const KeyByType &value) -> Iterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
    detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator it = bucket->Find(value);

    if (it == bucket->End()) {
        return End();
    }

    return Iterator(this, it);
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Find(const KeyByType &value) const -> ConstIterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
    const detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    const typename detail::HashSetBucket<ValueType, KeyByFunction>::ConstIterator it = bucket->Find(value);

    if (it == bucket->End()) {
        return End();
    }

    return ConstIterator(this, it);
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Erase(ConstIterator iter) -> Iterator
{
    if (iter == End()) {
        return End();
    }

    --m_size;

    detail::HashSetElement<ValueType, KeyByFunction> *prev = nullptr;

    for (auto it = iter.bucket_iter.bucket->head; it != nullptr && it != iter.bucket_iter.element; it = it->next) {
        prev = it;
    }

    if (iter.bucket_iter.element == iter.bucket_iter.bucket->head) {
        const_cast<detail::HashSetBucket<ValueType, KeyByFunction> *>(iter.bucket_iter.bucket)->head = iter.bucket_iter.element->next;
    }

    if (iter.bucket_iter.element == iter.bucket_iter.bucket->tail) {
        const_cast<detail::HashSetBucket<ValueType, KeyByFunction> *>(iter.bucket_iter.bucket)->tail = prev;
    }

    if (prev != nullptr) {
        prev->next = iter.bucket_iter.element->next;
    }

    Iterator next_iterator(this, typename detail::HashSetBucket<ValueType, KeyByFunction>::Iterator { const_cast<detail::HashSetBucket<ValueType, KeyByFunction> *>(iter.bucket_iter.bucket), iter.bucket_iter.element->next });

    delete iter.bucket_iter.element;

    return next_iterator;
}

template <class ValueType, auto KeyByFunction>
bool HashSet<ValueType, KeyByFunction>::Erase(const KeyByType &value)
{
    const Iterator it = Find(value);

    if (it == End()) {
        return false;
    }
    
    Erase(it);

    return true;
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Set(const ValueType &value) -> InsertResult
{
    const HashCode::ValueType hash_code = GetHashCode(value).Value();

    detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        *it = value;

        return InsertResult { insert_it, false };
    } else {
        insert_it.bucket_iter = bucket->Push(new detail::HashSetElement<ValueType, KeyByFunction> {
            nullptr,
            hash_code,
            value
        });

        m_size++;

        CheckAndRebuildBuckets();

        return InsertResult { insert_it, true };
    }
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Set(ValueType &&value) -> InsertResult
{
    const HashCode::ValueType hash_code = GetHashCode(value).Value();

    detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        *it = std::move(value);

        return InsertResult { insert_it, false };
    } else {
        insert_it.bucket_iter = bucket->Push(new detail::HashSetElement<ValueType, KeyByFunction> {
            nullptr,
            hash_code,
            std::move(value)
        });

        m_size++;

        CheckAndRebuildBuckets();

        return InsertResult { insert_it, true };
    }
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Insert(const ValueType &value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets();

    const HashCode::ValueType hash_code = GetHashCode(value).Value();

    detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        return InsertResult { insert_it, false };
    }

    insert_it.bucket_iter = bucket->Push(new detail::HashSetElement<ValueType, KeyByFunction> {
        nullptr,
        hash_code,
        value
    });
    
    m_size++;

    return InsertResult { insert_it, true };
}

template <class ValueType, auto KeyByFunction>
auto HashSet<ValueType, KeyByFunction>::Insert(ValueType &&value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets();

    const HashCode::ValueType hash_code = GetHashCode(value).Value();

    detail::HashSetBucket<ValueType, KeyByFunction> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(KeyByFunctionInvoker<KeyByFunction>()(value));

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        return InsertResult { insert_it, false };
    }

    insert_it.bucket_iter = bucket->Push(new detail::HashSetElement<ValueType, KeyByFunction> {
        nullptr,
        hash_code,
        std::move(value)
    });
    
    m_size++;

    return InsertResult { insert_it, true };
}

template <class ValueType, auto KeyByFunction>
void HashSet<ValueType, KeyByFunction>::Clear()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }

    m_buckets.Clear();
    m_buckets.Resize(initial_bucket_size);

    m_size = 0;
}

} // namespace containers

using containers::HashSet;

} // namespace hyperion

#endif
