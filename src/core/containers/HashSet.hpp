/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HASH_SET_HPP
#define HYPERION_HASH_SET_HPP

#include <core/containers/Array.hpp>
#include <core/containers/ContainerBase.hpp>

#include <core/utilities/Span.hpp>

#include <core/functional/FunctionWrapper.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace containers {
namespace detail {

template <class Value>
struct HashSetElement
{
    Value value;
    HashSetElement* next = nullptr;

    template <class... Args>
    HashSetElement(Args&&... args)
        : value(std::forward<Args>(args)...)
    {
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }
};

template <class Value>
struct HashSetBucket
{
    struct ConstIterator;

    struct Iterator
    {
        HashSetBucket<Value>* bucket;
        HashSetElement<Value>* element;

        HYP_FORCE_INLINE Value* operator->() const
        {
            return &element->value;
        }

        HYP_FORCE_INLINE Value& operator*() const
        {
            return element->value;
        }

        HYP_FORCE_INLINE Iterator& operator++()
        {
            element = element->next;
            return *this;
        }

        HYP_FORCE_INLINE Iterator operator++(int) const
        {
            return Iterator { bucket, element->next };
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return element != other.element;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return element != other.element;
        }

        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return { bucket, element };
        }
    };

    struct ConstIterator
    {
        const HashSetBucket<Value>* bucket;
        const HashSetElement<Value>* element;

        HYP_FORCE_INLINE const Value* operator->() const
        {
            return &element->value;
        }

        HYP_FORCE_INLINE const Value& operator*() const
        {
            return element->value;
        }

        HYP_FORCE_INLINE ConstIterator& operator++()
        {
            element = element->next;
            return *this;
        }

        HYP_FORCE_INLINE ConstIterator operator++(int) const
        {
            return ConstIterator { bucket, element->next };
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return element != other.element;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return element != other.element;
        }
    };

    HashSetElement<Value>* head;

    Iterator Push(HashSetElement<Value>* element)
    {
        HashSetElement<Value>* tail = head;

        if (head == nullptr)
        {
            head = element;
        }
        else
        {
            while (tail->next != nullptr)
            {
                tail = tail->next;
            }

            tail->next = element;
        }

        return { this, element };
    }

    template <class KeyByFunction, class TFindAsType>
    Iterator Find(KeyByFunction&& key_by_fn, const TFindAsType& value)
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (key_by_fn(it->value) == value)
            {
                return Iterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction, class TFindAsType>
    ConstIterator Find(KeyByFunction&& key_by_fn, const TFindAsType& value) const
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (key_by_fn(it->value) == value)
            {
                return ConstIterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction>
    Iterator FindByHashCode(KeyByFunction&& key_by_fn, HashCode::ValueType hash)
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (HashCode::GetHashCode(key_by_fn(it->value)).Value() == hash)
            {
                return Iterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction>
    ConstIterator FindByHashCode(KeyByFunction&& key_by_fn, HashCode::ValueType hash) const
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (HashCode::GetHashCode(key_by_fn(it->value)).Value() == hash)
            {
                return ConstIterator { this, it };
            }
        }

        return End();
    }

    Iterator Begin()
    {
        return { this, head };
    }

    Iterator End()
    {
        return { this, nullptr };
    }

    ConstIterator Begin() const
    {
        return { this, head };
    }

    ConstIterator End() const
    {
        return { this, nullptr };
    }

    Iterator begin()
    {
        return Begin();
    }

    Iterator end()
    {
        return End();
    }

    ConstIterator begin() const
    {
        return Begin();
    }

    ConstIterator end() const
    {
        return End();
    }

    ConstIterator cbegin() const
    {
        return Begin();
    }

    ConstIterator cend() const
    {
        return End();
    }
};

} // namespace detail

template <class Value, class ArrayAllocatorType = typename containers::detail::ArrayDefaultAllocatorSelector<Value, 32>::Type>
struct HashTable_PooledNodeAllocator
{
    using Node = detail::HashSetElement<Value>;
    using Bucket = detail::HashSetBucket<Value>;

    Array<Node, ArrayAllocatorType> m_pool;
    Node* m_free_nodes_head = nullptr;

    template <class... Args>
    Node* Allocate(Args&&... args)
    {
        if (m_free_nodes_head != nullptr)
        {
            Node* ptr = m_free_nodes_head;
            m_free_nodes_head = ptr->next;

            ptr->value = Value(std::forward<Args>(args)...);
            ptr->next = nullptr;

            return ptr;
        }

        AssertDebugMsg(m_pool.Capacity() >= m_pool.Size() + 1, "Allocate() call would invalidate element pointers - Capacity should be updated before this call");

        return &m_pool.EmplaceBack(std::forward<Args>(args)...);
    }

    void Free(Node* node)
    {
        AssertDebugMsg(node != nullptr, "Cannot free a null node");

        // Set the value to a default value
        node->value = Value();
        node->next = m_free_nodes_head;

        m_free_nodes_head = node;
    }

    void Fixup(const Node* previous_base, const Node* new_base, Span<Bucket> buckets)
    {
        if (!previous_base || previous_base == new_base)
        {
            return;
        }

        const auto shift = [previous_base, new_base](Node* p) -> Node*
        {
            if (!p)
            {
                return nullptr;
            }

            return const_cast<Node*>(new_base) + (p - const_cast<Node*>(previous_base));
        };

        for (Bucket& bucket : buckets)
        {
            bucket.head = shift(bucket.head);
        }

        if (m_free_nodes_head)
        {
            m_free_nodes_head = shift(m_free_nodes_head);
        }

        for (Node& n : m_pool)
        {
            if (n.next)
            {
                n.next = shift(n.next);
            }
        }
    }

    void Reserve(SizeType capacity, Span<Bucket> buckets)
    {
        if (capacity <= m_pool.Capacity())
        {
            return;
        }

        const Node* previous_base = m_pool.Any() ? m_pool.Data() : nullptr;
        m_pool.Reserve(capacity);

        Node* new_base = m_pool.Any() ? m_pool.Data() : nullptr;

        Fixup(previous_base, new_base, buckets);
    }

    void Swap(HashTable_PooledNodeAllocator& other, Span<Bucket> buckets)
    {
        std::swap(m_free_nodes_head, other.m_free_nodes_head);

        Node* previous_base = other.m_pool.Any() ? other.m_pool.Data() : nullptr;

        m_pool = std::move(other.m_pool);

        Node* new_base = m_pool.Any() ? m_pool.Data() : nullptr;

        Fixup(previous_base, new_base, buckets);
    }
};

template <class Value>
struct HashTable_DynamicNodeAllocator
{
    using Node = detail::HashSetElement<Value>;
    using Bucket = detail::HashSetBucket<Value>;

    template <class... Args>
    HYP_FORCE_INLINE Node* Allocate(Args&&... args)
    {
        return new Node(std::forward<Args>(args)...);
    }

    HYP_FORCE_INLINE void Free(Node* node)
    {
        delete node;
    }

    HYP_FORCE_INLINE void Reserve(SizeType capacity, Span<Bucket> buckets)
    {
        return; // No-op for dynamic allocation
    }

    HYP_FORCE_INLINE void Swap(HashTable_DynamicNodeAllocator& other, Span<Bucket> buckets)
    {
        // No-op for dynamic allocation
    }
};

template <class Value>
using HashTable_DefaultNodeAllocator = HashTable_PooledNodeAllocator<Value>;

template <class Value, auto KeyBy = &KeyBy_Identity<Value>, class NodeAllocatorType = HashTable_DefaultNodeAllocator<Value>>
class HashSet : public ContainerBase<HashSet<Value, KeyBy, NodeAllocatorType>, decltype(std::declval<FunctionWrapper<decltype(KeyBy)>>()(std::declval<Value>()))>
{
public:
    static constexpr bool is_contiguous = false;

protected:
    static constexpr SizeType initial_bucket_size = 8;
    static constexpr double desired_load_factor = 0.75;

    static constexpr FunctionWrapper<decltype(KeyBy)> key_by_fn { KeyBy };

    using Node = detail::HashSetElement<Value>;
    using Bucket = detail::HashSetBucket<Value>;

    using BucketArray = Array<Bucket, InlineAllocator<initial_bucket_size>>;

    template <class IteratorType>
    static inline void AdvanceIteratorBucket(IteratorType& iter)
    {
        const auto* end = iter.hash_set->m_buckets.End();

        while (iter.bucket_iter.element == nullptr && iter.bucket_iter.bucket != end)
        {
            if (++iter.bucket_iter.bucket == end)
            {
                break;
            }

            iter.bucket_iter.element = iter.bucket_iter.bucket->head;
        }
    }

    template <class IteratorType>
    static inline void AdvanceIterator(IteratorType& iter)
    {
        iter.bucket_iter.element = iter.bucket_iter.element->next;

        AdvanceIteratorBucket(iter);
    }

public:
    using KeyType = decltype(std::declval<FunctionWrapper<decltype(KeyBy)>>()(std::declval<Value>()));
    using ValueType = Value;

    using Base = ContainerBase<HashSet<Value, KeyBy, NodeAllocatorType>, KeyType>;

    struct ConstIterator;

    struct Iterator
    {
        HashSet* hash_set;
        typename Bucket::Iterator bucket_iter;

        Iterator(HashSet* hash_set, typename Bucket::Iterator bucket_iter)
            : hash_set(hash_set),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        Iterator(const Iterator& other) = default;
        Iterator& operator=(const Iterator& other) = default;
        Iterator(Iterator&& other) noexcept = default;
        Iterator& operator=(Iterator& other) & noexcept = default;
        ~Iterator() = default;

        HYP_FORCE_INLINE Value* operator->() const
        {
            return bucket_iter.operator->();
        }

        HYP_FORCE_INLINE Value& operator*() const
        {
            return bucket_iter.operator*();
        }

        HYP_FORCE_INLINE Iterator& operator++()
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

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return bucket_iter == other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return bucket_iter != other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return bucket_iter == other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return bucket_iter != other.bucket_iter;
        }

        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return ConstIterator { const_cast<const HashSet*>(hash_set), typename Bucket::ConstIterator(bucket_iter) };
        }
    };

    struct ConstIterator
    {
        const HashSet* hash_set;
        typename Bucket::ConstIterator bucket_iter;

        ConstIterator(const HashSet* hash_set, typename Bucket::ConstIterator bucket_iter)
            : hash_set(hash_set),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        ConstIterator(const ConstIterator& other) = default;
        ConstIterator& operator=(const ConstIterator& other) = default;
        ConstIterator(ConstIterator&& other) noexcept = default;
        ConstIterator& operator=(ConstIterator& other) & noexcept = default;
        ~ConstIterator() = default;

        HYP_FORCE_INLINE const Value* operator->() const
        {
            return bucket_iter.operator->();
        }

        HYP_FORCE_INLINE const Value& operator*() const
        {
            return bucket_iter.operator*();
        }

        HYP_FORCE_INLINE ConstIterator& operator++()
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

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return bucket_iter == other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return bucket_iter != other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return bucket_iter == other.bucket_iter;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return bucket_iter != other.bucket_iter;
        }
    };

    using InsertResult = Pair<Iterator, bool>;

    HashSet();

    HashSet(std::initializer_list<Value> initializer_list)
        : HashSet()
    {
        Array<Value> temp(initializer_list);

        for (auto&& item : temp)
        {
            Set(std::move(item));
        }
    }

    HashSet(const HashSet& other);
    HashSet& operator=(const HashSet& other);
    HashSet(HashSet&& other) noexcept;
    HashSet& operator=(HashSet&& other) noexcept;
    ~HashSet();

    HYP_FORCE_INLINE bool Any() const
    {
        return m_size != 0;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_size == 0;
    }

    HYP_FORCE_INLINE ValueType& Front()
    {
        AssertDebug(m_size != 0);
        return *Begin();
    }

    HYP_FORCE_INLINE const ValueType& Front() const
    {
        AssertDebug(m_size != 0);
        return *Begin();
    }

    HYP_FORCE_INLINE bool operator==(const HashSet& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const HashSet& other) const = delete;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE SizeType BucketCount() const
    {
        return m_buckets.Size();
    }

    HYP_FORCE_INLINE double LoadFactor(SizeType size) const
    {
        return double(size) / double(BucketCount());
    }

    HYP_FORCE_INLINE static constexpr double MaxLoadFactor()
    {
        return desired_load_factor;
    }

    void Reserve(SizeType capacity);

    Iterator Find(const KeyType& value);
    ConstIterator Find(const KeyType& value) const;

    template <class TFindAsType>
    Iterator FindAs(const TFindAsType& value)
    {
        const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
        Bucket* bucket = GetBucketForHash(hash_code);

        typename Bucket::Iterator it = bucket->Find(key_by_fn, value);

        if (it == bucket->End())
        {
            return End();
        }

        return Iterator(this, it);
    }

    template <class TFindAsType>
    ConstIterator FindAs(const TFindAsType& value) const
    {
        const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
        const Bucket* bucket = GetBucketForHash(hash_code);

        const typename Bucket::ConstIterator it = bucket->Find(key_by_fn, value);

        if (it == bucket->End())
        {
            return End();
        }

        return ConstIterator(this, it);
    }

    HYP_FORCE_INLINE bool Contains(const KeyType& value) const
    {
        return Find(value) != End();
    }

    HYP_FORCE_INLINE Value& At(const KeyType& value)
    {
        auto it = Find(value);
        AssertDebugMsg(it != End(), "At(): element not found");

        return *it;
    }

    HYP_FORCE_INLINE const Value& At(const KeyType& value) const
    {
        auto it = Find(value);
        AssertDebugMsg(it != End(), "At(): element not found");

        return *it;
    }

    Iterator Erase(ConstIterator iter);
    bool Erase(const KeyType& value);

    InsertResult Set(const Value& value);
    InsertResult Set(Value&& value);

    InsertResult Insert(const Value& value);
    InsertResult Insert(Value&& value);

    template <class... Args>
    HYP_FORCE_INLINE InsertResult Emplace(Args&&... args)
    {
        return Insert(Value(std::forward<Args>(args)...));
    }

    void Clear();

    template <class OtherContainerType>
    HashSet& Merge(const OtherContainerType& other)
    {
        for (const auto& item : other)
        {
            Insert(item);
        }

        return *this;
    }

    template <class OtherContainerType>
    HashSet& Merge(OtherContainerType&& other)
    {
        for (auto& item : other)
        {
            Insert(std::move(item));
        }

        other.Clear();

        return *this;
    }

    HYP_FORCE_INLINE Iterator Begin()
    {
        return Iterator(this, typename Bucket::Iterator { m_buckets.Data(), m_buckets[0].head });
    }

    HYP_FORCE_INLINE Iterator End()
    {
        return Iterator(this, typename Bucket::Iterator { m_buckets.Data() + m_buckets.Size(), nullptr });
    }

    HYP_FORCE_INLINE ConstIterator Begin() const
    {
        return ConstIterator(this, typename Bucket::ConstIterator { m_buckets.Data(), m_buckets[0].head });
    }

    HYP_FORCE_INLINE ConstIterator End() const
    {
        return ConstIterator(this, typename Bucket::ConstIterator { m_buckets.Data() + m_buckets.Size(), nullptr });
    }

    HYP_FORCE_INLINE Iterator begin()
    {
        return Begin();
    }

    HYP_FORCE_INLINE Iterator end()
    {
        return End();
    }

    HYP_FORCE_INLINE ConstIterator begin() const
    {
        return Begin();
    }

    HYP_FORCE_INLINE ConstIterator end() const
    {
        return End();
    }

    HYP_FORCE_INLINE ConstIterator cbegin() const
    {
        return Begin();
    }

    HYP_FORCE_INLINE ConstIterator cend() const
    {
        return End();
    }

protected:
    HYP_FORCE_INLINE static HashCode GetHashCodeForValue(const Value& value)
    {
        return HashCode::GetHashCode(key_by_fn(value));
    }

    void CheckAndRebuildBuckets(SizeType needed_capacity);

    HYP_FORCE_INLINE Bucket* GetBucketForHash(HashCode::ValueType hash)
    {
        return &m_buckets[hash % m_buckets.Size()];
    }

    HYP_FORCE_INLINE const Bucket* GetBucketForHash(HashCode::ValueType hash) const
    {
        return &m_buckets[hash % m_buckets.Size()];
    }

    BucketArray m_buckets;
    SizeType m_size;
    NodeAllocatorType m_node_allocator;
};

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet()
    : m_size(0)
{
    m_buckets.ResizeZeroed(initial_bucket_size);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(const HashSet& other)
    : m_size(other.m_size)
{
    m_node_allocator.Reserve(m_size, m_buckets);

    m_buckets.ResizeZeroed(other.m_buckets.Size());

    if (m_size != 0)
    {
        for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++)
        {
            const auto& bucket = other.m_buckets[bucket_index];

            for (auto it = bucket.head; it != nullptr; it = it->next)
            {
                Node* ptr = m_node_allocator.Allocate(it->value);

                m_buckets[bucket_index].Push(ptr);
            }
        }
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::operator=(const HashSet& other) -> HashSet&
{
    if (this == &other)
    {
        return *this;
    }

    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it)
    {
        for (auto element_it = buckets_it->head; element_it != nullptr;)
        {
            auto* head = element_it;
            auto* next = head->next;

            m_node_allocator.Free(head);

            element_it = next;
        }
    }

    m_size = other.m_size;
    m_buckets.Clear();

    m_node_allocator.Reserve(m_size, m_buckets);

    m_buckets.ResizeZeroed(other.m_buckets.Size());

    for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++)
    {
        const auto& bucket = other.m_buckets[bucket_index];

        for (auto it = bucket.head; it != nullptr; it = it->next)
        {
            Node* ptr = m_node_allocator.Allocate(it->value);

            m_buckets[bucket_index].Push(ptr);
        }
    }

    return *this;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(HashSet&& other) noexcept
    : m_buckets(std::move(other.m_buckets)),
      m_size(other.m_size)
{
    m_node_allocator.Swap(other.m_node_allocator, m_buckets);

    other.m_size = 0;
    other.m_buckets.ResizeZeroed(initial_bucket_size);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::operator=(HashSet&& other) noexcept -> HashSet&
{
    if (&other == this)
    {
        return *this;
    }

    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it)
    {
        for (auto element_it = buckets_it->head; element_it != nullptr;)
        {
            auto* head = element_it;
            auto* next = head->next;

            m_node_allocator.Free(head);

            element_it = next;
        }
    }

    m_buckets = std::move(other.m_buckets);
    other.m_buckets.ResizeZeroed(initial_bucket_size);

    m_size = other.m_size;
    other.m_size = 0;

    m_node_allocator.Swap(other.m_node_allocator, m_buckets);

    return *this;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::~HashSet()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it)
    {
        for (auto element_it = buckets_it->head; element_it != nullptr;)
        {
            auto* head = element_it;
            auto* next = head->next;

            m_node_allocator.Free(head);

            element_it = next;
        }
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::Reserve(SizeType capacity)
{
    m_node_allocator.Reserve(capacity, m_buckets);

    const SizeType new_bucket_count = SizeType(MathUtil::Ceil(double(capacity) / MaxLoadFactor()));

    if (new_bucket_count <= m_buckets.Size())
    {
        return;
    }

    BucketArray new_buckets;
    new_buckets.ResizeZeroed(new_bucket_count);

    for (auto& bucket : m_buckets)
    {
        Node* next = nullptr;

        for (auto it = bucket.head; it != nullptr;)
        {
            next = it->next;
            it->next = nullptr;

            Bucket* new_bucket = &new_buckets[GetHashCodeForValue(it->value).Value() % new_buckets.Size()];

            new_bucket->Push(it);

            it = next;
        }
    }

    m_buckets = std::move(new_buckets);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::CheckAndRebuildBuckets(SizeType needed_capacity)
{
    // Check load factor, if currently load factor is greater than `load_factor`, then rehash so that the load factor becomes <= `load_factor` constant.

    if (LoadFactor(needed_capacity) < MaxLoadFactor())
    {
        m_node_allocator.Reserve(needed_capacity, m_buckets);

        return;
    }

    Reserve(needed_capacity * 2);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Find(const KeyType& value) -> Iterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
    Bucket* bucket = GetBucketForHash(hash_code);

    typename Bucket::Iterator it = bucket->Find(key_by_fn, value);

    if (it == bucket->End())
    {
        return End();
    }

    return Iterator(this, it);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Find(const KeyType& value) const -> ConstIterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(value).Value();
    const Bucket* bucket = GetBucketForHash(hash_code);

    const typename Bucket::ConstIterator it = bucket->Find(key_by_fn, value);

    if (it == bucket->End())
    {
        return End();
    }

    return ConstIterator(this, it);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Erase(ConstIterator iter) -> Iterator
{
    if (iter == End())
    {
        return End();
    }

    --m_size;

    Node* prev = nullptr;

    for (auto it = iter.bucket_iter.bucket->head; it != nullptr && it != iter.bucket_iter.element; it = it->next)
    {
        prev = it;
    }

    if (iter.bucket_iter.element == iter.bucket_iter.bucket->head)
    {
        const_cast<Bucket*>(iter.bucket_iter.bucket)->head = iter.bucket_iter.element->next;
    }

    if (prev != nullptr)
    {
        prev->next = iter.bucket_iter.element->next;
    }

    Iterator next_iterator(this, typename Bucket::Iterator { const_cast<Bucket*>(iter.bucket_iter.bucket), iter.bucket_iter.element->next });

    m_node_allocator.Free(const_cast<Node*>(iter.bucket_iter.element));

    return next_iterator;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
bool HashSet<Value, KeyBy, NodeAllocatorType>::Erase(const KeyType& value)
{
    const Iterator it = Find(value);

    if (it == End())
    {
        return false;
    }

    Erase(it);

    return true;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Set(const Value& value) -> InsertResult
{
    const HashCode::ValueType hash_code = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(key_by_fn, key_by_fn(value));

    Iterator insert_it(this, it);

    if (it != bucket->End())
    {
        *it = value;

        return InsertResult { insert_it, false };
    }
    else
    {
        CheckAndRebuildBuckets(m_size + 1);
        bucket = GetBucketForHash(hash_code);

        Node* ptr = m_node_allocator.Allocate(value);

        insert_it.bucket_iter = bucket->Push(ptr);

        m_size++;

        return InsertResult { insert_it, true };
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Set(Value&& value) -> InsertResult
{
    const HashCode::ValueType hash_code = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(key_by_fn, key_by_fn(value));

    Iterator insert_it(this, it);

    if (it != bucket->End())
    {
        *it = std::move(value);

        return InsertResult { insert_it, false };
    }
    else
    {
        CheckAndRebuildBuckets(m_size + 1);
        bucket = GetBucketForHash(hash_code);

        Node* ptr = m_node_allocator.Allocate(std::move(value));

        insert_it.bucket_iter = bucket->Push(ptr);

        m_size++;

        return InsertResult { insert_it, true };
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Insert(const Value& value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets(m_size + 1);

    const HashCode::ValueType hash_code = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(key_by_fn, key_by_fn(value));

    Iterator insert_it(this, it);

    if (it != bucket->End())
    {
        return InsertResult { insert_it, false };
    }

    Node* ptr = m_node_allocator.Allocate(value);

    insert_it.bucket_iter = bucket->Push(ptr);

    m_size++;

    return InsertResult { insert_it, true };
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Insert(Value&& value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets(m_size + 1);

    const HashCode::ValueType hash_code = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(key_by_fn, key_by_fn(value));

    Iterator insert_it(this, it);

    if (it != bucket->End())
    {
        return InsertResult { insert_it, false };
    }

    Node* ptr = m_node_allocator.Allocate(std::move(value));

    insert_it.bucket_iter = bucket->Push(ptr);
    m_size++;

    return InsertResult { insert_it, true };
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::Clear()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it)
    {
        for (auto element_it = buckets_it->head; element_it != nullptr;)
        {
            auto* head = element_it;
            auto* next = head->next;

            m_node_allocator.Free(head);

            element_it = next;
        }
    }

    m_buckets.Clear();
    m_buckets.ResizeZeroed(initial_bucket_size);

    m_size = 0;
}

} // namespace containers

using containers::HashSet;
using containers::HashTable_DefaultNodeAllocator;
using containers::HashTable_DynamicNodeAllocator;
using containers::HashTable_PooledNodeAllocator;

} // namespace hyperion

#endif
