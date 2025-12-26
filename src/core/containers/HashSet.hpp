/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/ContainerBase.hpp>

#include <core/utilities/Span.hpp>

#include <core/functional/FunctionWrapper.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/utilities/Traits.hpp>
#include <core/HashCode.hpp>

namespace Hyperion {

namespace containers {
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

        HYP_FORCE_INLINE bool operator<(const ConstIterator& other) const
        {
            if (bucket != other.bucket)
            {
                return bucket < other.bucket;
            }

            return element < other.element;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return element != other.element;
        }

        HYP_FORCE_INLINE bool operator<(const Iterator& other) const
        {
            if (bucket != other.bucket)
            {
                return bucket < other.bucket;
            }

            return element < other.element;
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

        HYP_FORCE_INLINE bool operator<(const ConstIterator& other) const
        {
            if (bucket != other.bucket)
            {
                return bucket < other.bucket;
            }

            return element < other.element;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return element == other.element;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return element != other.element;
        }

        HYP_FORCE_INLINE bool operator<(const Iterator& other) const
        {
            if (bucket != other.bucket)
            {
                return bucket < other.bucket;
            }

            return element < other.element;
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
    Iterator Find(KeyByFunction&& keyByFn, const TFindAsType& value)
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (keyByFn(it->value) == value)
            {
                return Iterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction, class TFindAsType>
    ConstIterator Find(KeyByFunction&& keyByFn, const TFindAsType& value) const
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (keyByFn(it->value) == value)
            {
                return ConstIterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction>
    Iterator FindByHashCode(KeyByFunction&& keyByFn, HashCode::ValueType hash)
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (HashCode::GetHashCode(keyByFn(it->value)).Value() == hash)
            {
                return Iterator { this, it };
            }
        }

        return End();
    }

    template <class KeyByFunction>
    ConstIterator FindByHashCode(KeyByFunction&& keyByFn, HashCode::ValueType hash) const
    {
        for (auto it = head; it != nullptr; it = it->next)
        {
            if (HashCode::GetHashCode(keyByFn(it->value)).Value() == hash)
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

template <class Allocator>
struct PooledNodeAllocator
{
    using AllocatorType = Allocator;

    template <class Value>
    struct Impl
    {
        using Node = HashSetElement<Value>;
        using Bucket = HashSetBucket<Value>;

        Node* m_freeNodesHead;
        Array<Node, Allocator> m_pool;

        Impl()
            : m_freeNodesHead(nullptr),
              m_pool()
        {
        }

        explicit Impl(Allocator* pAllocator)
            : m_freeNodesHead(nullptr),
              m_pool(pAllocator)
        {
        }

        Impl(const Impl& other) = delete;
        Impl& operator=(const Impl& other) = delete;

        Impl(Impl&& other) noexcept = delete;
        Impl& operator=(Impl&& other) noexcept = delete;

        template <class Ty>
        Node* Allocate(Ty&& value, Span<Bucket> buckets)
        {
            if (m_freeNodesHead != nullptr)
            {
                Node* ptr = m_freeNodesHead;
                m_freeNodesHead = ptr->next;

                ptr->value = std::forward<Ty>(value);
                ptr->next = nullptr;

                return ptr;
            }

            HYP_CORE_ASSERT(m_pool.Capacity() >= m_pool.Size() + 1, "Allocate() call would invalidate element pointers - Capacity should be updated before this call");

            Node* previousBase = m_pool.Data();

            Node* ptr = &m_pool.EmplaceBack(std::forward<Ty>(value));

            Node* newBase = m_pool.Data();

            HYP_CORE_ASSERT(previousBase == newBase, "Allocate() call would invalidate element pointers - Capacity should be updated before this call");

            return ptr;
        }

        void Free(Node* node)
        {
            HYP_CORE_ASSERT(node != nullptr, "Cannot free a null node");

            // Set the value to a default value
            node->value = Value();
            node->next = m_freeNodesHead;

            m_freeNodesHead = node;
        }

        void Fixup(const Node* previousBase, const Node* newBase, Span<Bucket> buckets)
        {
            if (!previousBase || previousBase == newBase)
            {
                return;
            }

            const auto shift = [previousBase, newBase](Node* p) -> Node*
            {
                if (!p)
                {
                    return nullptr;
                }

                return reinterpret_cast<Node*>(UIntPtr(newBase) + (UIntPtr(p) - UIntPtr(previousBase)));
            };

            for (Bucket& bucket : buckets)
            {
                bucket.head = shift(bucket.head);
            }

            if (m_freeNodesHead)
            {
                m_freeNodesHead = shift(m_freeNodesHead);
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

            const Node* previousBase = m_pool.Data();
            m_pool.Reserve(capacity);

            Node* newBase = m_pool.Data();

            Fixup(previousBase, newBase, buckets);
        }

        void Swap(Impl& other, Span<Bucket> buckets)
        {
            std::swap(m_freeNodesHead, other.m_freeNodesHead);

            Node* previousBase = other.m_pool.Data();

            m_pool = std::move(other.m_pool);

            Node* newBase = m_pool.Data();

            Fixup(previousBase, newBase, buckets);
        }
    };
};

template <class Allocator>
struct NodeAllocator
{
    using AllocatorType = Allocator;

    template <class Value>
    struct Impl
    {
        using Node = HashSetElement<Value>;
        using Bucket = HashSetBucket<Value>;

        Allocator* m_pAllocator;

        Impl()
            : m_pAllocator(GetDefaultAllocatorInstance<Allocator>())
        {
            HYP_CORE_ASSERT(m_pAllocator != nullptr);
        }

        explicit Impl(Allocator* pAllocator)
            : m_pAllocator(pAllocator)
        {
            HYP_CORE_ASSERT(m_pAllocator != nullptr);
        }

        Impl(const Impl& other) = delete;
        Impl& operator=(const Impl& other) = delete;

        Impl(Impl&& other) noexcept = delete;
        Impl& operator=(Impl&& other) noexcept = delete;

        template <class Ty>
        Node* Allocate(Ty&& value, Span<Bucket> /*buckets*/)
        {
            void* mem = m_pAllocator->Allocate(sizeof(Node), alignof(Node));
            HYP_CORE_ASSERT(mem != nullptr);
            return new (mem) Node(std::forward<Ty>(value));
        }

        HYP_FORCE_INLINE void Free(Node* node)
        {
            if (!node)
            {
                return;
            }
            node->~Node();
            m_pAllocator->Free(node);
        }

        HYP_FORCE_INLINE void Reserve(SizeType /*capacity*/, Span<Bucket> /*buckets*/)
        {
            // No-op for dynamic allocation
        }

        HYP_FORCE_INLINE void Swap(Impl& other, Span<Bucket> /*buckets*/)
        {
            std::swap(m_pAllocator, other.m_pAllocator);
        }
    };
};

template <class T>
struct IsPooledNodeAllocator : std::false_type
{
};

template <class Allocator>
struct IsPooledNodeAllocator<PooledNodeAllocator<Allocator>> : std::true_type
{
};

using DynamicNodeAllocator = NodeAllocator<DynamicAllocator>;
using DefaultNodeAllocator = PooledNodeAllocator<DynamicAllocator>;

/*! \brief A hash set container that uses a hash table to store unique values, supporting a custom node allocator and a key extraction function.
 *  \details This container allows for efficient storage and retrieval of unique values based on a key extracted from each value using the provided `KeyBy` function. The default key extraction function is the identity function, which uses the value itself as the key.
 *  \tparam Value The type of values stored in the hash set.
 *  \tparam KeyBy A function that extracts a key from a value. The default is the identity function, which uses the value itself as the key.
 *  \tparam NodeAllocatorType The type of node allocator used for managing memory for the hash set elements. The default is `DefaultNodeAllocator`, which can leverage pooled allocation for reduced dynamic memory allocation. If you want to use dynamic allocation (e.g need stable pointers to elements), you can use `DynamicNodeAllocator<Value>` instead.
 */
template <class Value, auto KeyBy = &KeyBy_Identity<Value>, class NodeAllocatorType = DefaultNodeAllocator>
class HashSet : public ContainerBase<HashSet<Value, KeyBy, NodeAllocatorType>, decltype(std::declval<FunctionWrapper<decltype(KeyBy)>>()(std::declval<const Value&>()))>
{
public:
    static constexpr bool isContiguous = false;

    static constexpr SizeType InitialBucketSize = 16;
    static constexpr double DesiredLoadFactor = 0.75;

    using Node = HashSetElement<Value>;
    using Bucket = HashSetBucket<Value>;

    using BucketArray = Array<Bucket, InlineAllocator<InitialBucketSize>>;

protected:
    static constexpr FunctionWrapper<decltype(KeyBy)> keyByFn { KeyBy };

    static_assert(std::is_trivial_v<Bucket>, "Bucket must be a trivial type");

    template <class IteratorType>
    static inline void AdvanceIteratorBucket(IteratorType& iter)
    {
        const auto* end = iter.hashSet->m_buckets.End();

        while (iter.bucketIter.element == nullptr && iter.bucketIter.bucket != end)
        {
            if (++iter.bucketIter.bucket == end)
            {
                break;
            }

            iter.bucketIter.element = iter.bucketIter.bucket->head;
        }
    }

    template <class IteratorType>
    static inline void AdvanceIterator(IteratorType& iter)
    {
        iter.bucketIter.element = iter.bucketIter.element->next;

        AdvanceIteratorBucket(iter);
    }

public:
    using KeyType = decltype(std::declval<FunctionWrapper<decltype(KeyBy)>>()(std::declval<const Value&>()));
    using ValueType = Value;

    using Base = ContainerBase<HashSet<Value, KeyBy, NodeAllocatorType>, KeyType>;

    struct ConstIterator;

    struct Iterator
    {
        HashSet* hashSet;
        typename Bucket::Iterator bucketIter;

        Iterator(HashSet* hashSet, typename Bucket::Iterator bucketIter)
            : hashSet(hashSet),
              bucketIter(bucketIter)
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
            return bucketIter.operator->();
        }

        HYP_FORCE_INLINE Value& operator*() const
        {
            return bucketIter.operator*();
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
            return bucketIter == other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return bucketIter != other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator<(const Iterator& other) const
        {
            return bucketIter < other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return bucketIter == other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return bucketIter != other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator<(const ConstIterator& other) const
        {
            return bucketIter < other.bucketIter;
        }

        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return ConstIterator { const_cast<const HashSet*>(hashSet), typename Bucket::ConstIterator(bucketIter) };
        }
    };

    struct ConstIterator
    {
        const HashSet* hashSet;
        typename Bucket::ConstIterator bucketIter;

        ConstIterator(const HashSet* hashSet, typename Bucket::ConstIterator bucketIter)
            : hashSet(hashSet),
              bucketIter(bucketIter)
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
            return bucketIter.operator->();
        }

        HYP_FORCE_INLINE const Value& operator*() const
        {
            return bucketIter.operator*();
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
            return bucketIter == other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return bucketIter != other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator<(const Iterator& other) const
        {
            return bucketIter < other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return bucketIter == other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return bucketIter != other.bucketIter;
        }

        HYP_FORCE_INLINE bool operator<(const ConstIterator& other) const
        {
            return bucketIter < other.bucketIter;
        }
    };

    using InsertResult = Pair<Iterator, bool>;

    HashSet();
    HashSet(std::initializer_list<Value> initializerList);
    explicit HashSet(typename NodeAllocatorType::AllocatorType* pAllocator);
    HashSet(typename NodeAllocatorType::AllocatorType* pAllocator, std::initializer_list<Value> initializerList);

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
        HYP_CORE_ASSERT(m_size != 0);
        return *Begin();
    }

    HYP_FORCE_INLINE const ValueType& Front() const
    {
        HYP_CORE_ASSERT(m_size != 0);
        return *Begin();
    }

    HYP_FORCE_INLINE bool operator==(const HashSet& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const HashSet& other) const = delete;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE SizeType Capacity() const
    {
        if constexpr (IsPooledNodeAllocator<NodeAllocatorType>::value)
        {
            return m_nodeAllocator.m_pool.Capacity();
        }
        else
        {
            return SizeType(-1); // Dynamic allocators do not have a capacity
        }
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
        return DesiredLoadFactor;
    }

    void Reserve(SizeType capacity);

    Iterator Find(const KeyType& value);
    ConstIterator Find(const KeyType& value) const;

    template <class TFindAsType>
    Iterator FindAs(const TFindAsType& value)
    {
        const HashCode::ValueType hashCode = HashCode::GetHashCode(value).Value();
        Bucket* bucket = GetBucketForHash(hashCode);

        typename Bucket::Iterator it = bucket->Find(keyByFn, value);

        if (it == bucket->End())
        {
            return End();
        }

        return Iterator(this, it);
    }

    template <class TFindAsType>
    ConstIterator FindAs(const TFindAsType& value) const
    {
        const HashCode::ValueType hashCode = HashCode::GetHashCode(value).Value();
        const Bucket* bucket = GetBucketForHash(hashCode);

        const typename Bucket::ConstIterator it = bucket->Find(keyByFn, value);

        if (it == bucket->End())
        {
            return End();
        }

        return ConstIterator(this, it);
    }

    Iterator FindByHashCode(HashCode hashCode);
    ConstIterator FindByHashCode(HashCode hashCode) const;

    HYP_FORCE_INLINE ValueType& Get(const KeyType& value)
    {
        return At(value);
    }

    HYP_FORCE_INLINE const ValueType& Get(const KeyType& value) const
    {
        return At(value);
    }

    HYP_FORCE_INLINE ValueType* TryGet(const KeyType& value)
    {
        auto it = Find(value);

        if (it == End())
        {
            return nullptr;
        }

        return &(*it);
    }

    HYP_FORCE_INLINE const ValueType* TryGet(const KeyType& value) const
    {
        auto it = Find(value);

        if (it == End())
        {
            return nullptr;
        }

        return &(*it);
    }

    HYP_FORCE_INLINE bool Contains(const KeyType& value) const
    {
        return Find(value) != End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE SizeType Count(const TFindAsType& value) const
    {
        auto it = FindAs(value);

        if (it == End())
        {
            return 0;
        }

        SizeType count = 0;

        for (; it != End() && keyByFn(*it) == value; ++it)
        {
            ++count;
        }

        return count;
    }

    HYP_FORCE_INLINE Value& At(const KeyType& value)
    {
        auto it = Find(value);
        HYP_CORE_ASSERT(it != End(), "At(): element not found");

        return *it;
    }

    HYP_FORCE_INLINE const Value& At(const KeyType& value) const
    {
        auto it = Find(value);
        HYP_CORE_ASSERT(it != End(), "At(): element not found");

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

    /*! \brief Alias for Insert(), but returns the inserted value directly. */
    HYP_FORCE_INLINE Value& Add(const ValueType& value)
    {
        return *Insert(value).first;
    }

    /*! \brief Alias for Insert(), but returns the inserted value directly. */
    HYP_FORCE_INLINE Value& Add(ValueType&& value)
    {
        return *Insert(std::move(value)).first;
    }

    void Clear();

    template <class OtherContainerType>
    HashSet& Merge(const OtherContainerType& other)
    {
        for (const auto& item : other)
        {
            Set(item);
        }

        return *this;
    }

    template <class OtherContainerType, typename = std::enable_if_t<std::is_rvalue_reference_v<OtherContainerType>>>
    HashSet& Merge(OtherContainerType&& other)
    {
        for (auto& item : other)
        {
            Set(std::move(item));
        }

        other.Clear();

        return *this;
    }

    HYP_NODISCARD HYP_FORCE_INLINE Array<Value> ToArray() const&
    {
        Array<Value> result;
        result.ResizeUninitialized(m_size);

        SizeType index = 0;

        for (const auto& item : *this)
        {
            Memory::Construct<Value>(result.Data() + index++, item);
        }

        return result;
    }

    HYP_NODISCARD HYP_FORCE_INLINE Array<Value> ToArray() &&
    {
        Array<Value> result;
        result.ResizeUninitialized(m_size);

        SizeType index = 0;

        for (auto&& item : std::move(*this))
        {
            Memory::Construct<Value>(result.Data() + index++, std::move(item));
        }

        Clear();

        return result;
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
        return HashCode::GetHashCode(keyByFn(value));
    }

    void CheckAndRebuildBuckets(SizeType neededCapacity);

    HYP_FORCE_INLINE Bucket* GetBucketForHash(HashCode::ValueType hash)
    {
        return &m_buckets[hash % m_buckets.Size()];
    }

    HYP_FORCE_INLINE const Bucket* GetBucketForHash(HashCode::ValueType hash) const
    {
        return &m_buckets[hash % m_buckets.Size()];
    }

    BucketArray m_buckets;
    typename NodeAllocatorType::template Impl<Value> m_nodeAllocator;
    SizeType m_size;
};

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet()
    : m_size(0)
{
    m_buckets.ResizeZeroed(InitialBucketSize);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(std::initializer_list<Value> initializerList)
    : HashSet()
{
    for (const auto& item : initializerList)
    {
        Insert(item);
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(typename NodeAllocatorType::AllocatorType* pAllocator)
    : m_size(0),
      m_nodeAllocator(pAllocator)
{
    m_buckets.ResizeZeroed(InitialBucketSize);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(typename NodeAllocatorType::AllocatorType* pAllocator, std::initializer_list<Value> initializerList)
    : HashSet(pAllocator)
{
    for (const auto& item : initializerList)
    {
        Insert(item);
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(const HashSet& other)
    : m_size(other.m_size)
{
    m_nodeAllocator.Reserve(m_size, m_buckets);

    m_buckets.ResizeZeroed(other.m_buckets.Size());

    if (m_size != 0)
    {
        for (SizeType bucketIndex = 0; bucketIndex < other.m_buckets.Size(); bucketIndex++)
        {
            const auto& bucket = other.m_buckets[bucketIndex];

            for (auto it = bucket.head; it != nullptr; it = it->next)
            {
                Node* ptr = m_nodeAllocator.Allocate(it->value, m_buckets.ToSpan());

                m_buckets[bucketIndex].Push(ptr);
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

    for (auto bucketsIt = m_buckets.Begin(); bucketsIt != m_buckets.End(); ++bucketsIt)
    {
        for (auto elementIt = bucketsIt->head; elementIt != nullptr;)
        {
            auto* head = elementIt;
            auto* next = head->next;

            m_nodeAllocator.Free(head);

            elementIt = next;
        }
    }

    m_size = other.m_size;
    m_buckets.Clear();

    m_nodeAllocator.Reserve(m_size, m_buckets);

    m_buckets.ResizeZeroed(other.m_buckets.Size());

    for (SizeType bucketIndex = 0; bucketIndex < other.m_buckets.Size(); bucketIndex++)
    {
        const auto& bucket = other.m_buckets[bucketIndex];

        for (auto it = bucket.head; it != nullptr; it = it->next)
        {
            Node* ptr = m_nodeAllocator.Allocate(it->value, m_buckets.ToSpan());

            m_buckets[bucketIndex].Push(ptr);
        }
    }

    return *this;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::HashSet(HashSet&& other) noexcept
    : m_buckets(std::move(other.m_buckets)),
      m_size(other.m_size)
{
    m_nodeAllocator.Swap(other.m_nodeAllocator, m_buckets);

    other.m_size = 0;
    other.m_buckets.ResizeZeroed(InitialBucketSize);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::operator=(HashSet&& other) noexcept -> HashSet&
{
    if (&other == this)
    {
        return *this;
    }

    for (auto bucketsIt = m_buckets.Begin(); bucketsIt != m_buckets.End(); ++bucketsIt)
    {
        for (auto elementIt = bucketsIt->head; elementIt != nullptr;)
        {
            auto* head = elementIt;
            auto* next = head->next;

            m_nodeAllocator.Free(head);

            elementIt = next;
        }
    }

    m_buckets = std::move(other.m_buckets);
    other.m_buckets.ResizeZeroed(InitialBucketSize);

    m_size = other.m_size;
    other.m_size = 0;

    m_nodeAllocator.Swap(other.m_nodeAllocator, m_buckets);

    return *this;
}

template <class Value, auto KeyBy, class NodeAllocatorType>
HashSet<Value, KeyBy, NodeAllocatorType>::~HashSet()
{
    for (auto bucketsIt = m_buckets.Begin(); bucketsIt != m_buckets.End(); ++bucketsIt)
    {
        for (auto elementIt = bucketsIt->head; elementIt != nullptr;)
        {
            auto* head = elementIt;
            auto* next = head->next;

            m_nodeAllocator.Free(head);

            elementIt = next;
        }
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::Reserve(SizeType capacity)
{
    m_nodeAllocator.Reserve(capacity, m_buckets);

    const SizeType newBucketCount = SizeType(MathUtil::Ceil(double(capacity) / MaxLoadFactor()));

    if (newBucketCount <= m_buckets.Size())
    {
        return;
    }

    BucketArray newBuckets;
    newBuckets.ResizeZeroed(newBucketCount);

    for (auto& bucket : m_buckets)
    {
        Node* next = nullptr;

        for (auto it = bucket.head; it != nullptr;)
        {
            next = it->next;
            it->next = nullptr;

            Bucket* newBucket = &newBuckets[GetHashCodeForValue(it->value).Value() % newBuckets.Size()];

            newBucket->Push(it);

            it = next;
        }
    }

    m_buckets = std::move(newBuckets);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::CheckAndRebuildBuckets(SizeType neededCapacity)
{
    // Check load factor, if currently load factor is greater than `loadFactor`, then rehash so that the load factor becomes <= `loadFactor` constant.

    if (LoadFactor(neededCapacity) < MaxLoadFactor())
    {
        m_nodeAllocator.Reserve(neededCapacity, m_buckets);

        return;
    }

    Reserve(neededCapacity * 2);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Find(const KeyType& value) -> Iterator
{
    const HashCode::ValueType hashCode = HashCode::GetHashCode(value).Value();
    Bucket* bucket = GetBucketForHash(hashCode);

    typename Bucket::Iterator it = bucket->Find(keyByFn, value);

    if (it == bucket->End())
    {
        return End();
    }

    return Iterator(this, it);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Find(const KeyType& value) const -> ConstIterator
{
    const HashCode::ValueType hashCode = HashCode::GetHashCode(value).Value();
    const Bucket* bucket = GetBucketForHash(hashCode);

    const typename Bucket::ConstIterator it = bucket->Find(keyByFn, value);

    if (it == bucket->End())
    {
        return End();
    }

    return ConstIterator(this, it);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::FindByHashCode(HashCode hashCode) -> Iterator
{
    Bucket* bucket = GetBucketForHash(hashCode.Value());

    typename Bucket::Iterator it = bucket->FindByHashCode(keyByFn, hashCode.Value());

    if (it == bucket->End())
    {
        return End();
    }

    return Iterator(this, it);
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::FindByHashCode(HashCode hashCode) const -> ConstIterator
{
    const Bucket* bucket = GetBucketForHash(hashCode.Value());

    const typename Bucket::ConstIterator it = bucket->FindByHashCode(keyByFn, hashCode.Value());

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

    for (auto it = iter.bucketIter.bucket->head; it != nullptr && it != iter.bucketIter.element; it = it->next)
    {
        prev = it;
    }

    if (iter.bucketIter.element == iter.bucketIter.bucket->head)
    {
        const_cast<Bucket*>(iter.bucketIter.bucket)->head = iter.bucketIter.element->next;
    }

    if (prev != nullptr)
    {
        prev->next = iter.bucketIter.element->next;
    }

    Iterator nextIterator(this, typename Bucket::Iterator { const_cast<Bucket*>(iter.bucketIter.bucket), iter.bucketIter.element->next });

    m_nodeAllocator.Free(const_cast<Node*>(iter.bucketIter.element));

    return nextIterator;
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
    const HashCode::ValueType hashCode = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hashCode);

    auto it = bucket->Find(keyByFn, keyByFn(value));

    Iterator insertIt(this, it);

    if (it != bucket->End())
    {
        *it = value;

        return InsertResult { insertIt, false };
    }
    else
    {
        CheckAndRebuildBuckets(m_size + 1);
        bucket = GetBucketForHash(hashCode);

        Node* ptr = m_nodeAllocator.Allocate(value, m_buckets.ToSpan());

        insertIt.bucketIter = bucket->Push(ptr);

        m_size++;

        return InsertResult { insertIt, true };
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Set(Value&& value) -> InsertResult
{
    const HashCode::ValueType hashCode = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hashCode);

    auto it = bucket->Find(keyByFn, keyByFn(value));

    Iterator insertIt(this, it);

    if (it != bucket->End())
    {
        *it = std::move(value);

        return InsertResult { insertIt, false };
    }
    else
    {

        CheckAndRebuildBuckets(m_size + 1);
        bucket = GetBucketForHash(hashCode);
        Node* ptr = m_nodeAllocator.Allocate(std::move(value), m_buckets.ToSpan());
        insertIt.bucketIter = bucket->Push(ptr);

        m_size++;

        return InsertResult { insertIt, true };
    }
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Insert(const Value& value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets(m_size + 1);

    const HashCode::ValueType hashCode = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hashCode);

    auto it = bucket->Find(keyByFn, keyByFn(value));

    Iterator insertIt(this, it);

    if (it != bucket->End())
    {
        return InsertResult { insertIt, false };
    }

    Node* ptr = m_nodeAllocator.Allocate(value, m_buckets.ToSpan());

    insertIt.bucketIter = bucket->Push(ptr);

    m_size++;

    return InsertResult { insertIt, true };
}

template <class Value, auto KeyBy, class NodeAllocatorType>
auto HashSet<Value, KeyBy, NodeAllocatorType>::Insert(Value&& value) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets(m_size + 1);

    const HashCode::ValueType hashCode = GetHashCodeForValue(value).Value();

    Bucket* bucket = GetBucketForHash(hashCode);

    auto it = bucket->Find(keyByFn, keyByFn(value));

    Iterator insertIt(this, it);

    if (it != bucket->End())
    {
        return InsertResult { insertIt, false };
    }

    Node* ptr = m_nodeAllocator.Allocate(std::move(value), m_buckets.ToSpan());

    insertIt.bucketIter = bucket->Push(ptr);
    m_size++;

    return InsertResult { insertIt, true };
}

template <class Value, auto KeyBy, class NodeAllocatorType>
void HashSet<Value, KeyBy, NodeAllocatorType>::Clear()
{
    for (auto bucketsIt = m_buckets.Begin(); bucketsIt != m_buckets.End(); ++bucketsIt)
    {
        for (auto elementIt = bucketsIt->head; elementIt != nullptr;)
        {
            auto* head = elementIt;
            auto* next = head->next;

            m_nodeAllocator.Free(head);

            elementIt = next;
        }
    }

    m_buckets.Clear();
    m_buckets.ResizeZeroed(InitialBucketSize);

    m_size = 0;
}

} // namespace containers

using containers::DefaultNodeAllocator;
using containers::DynamicNodeAllocator;
using containers::HashSet;
using containers::NodeAllocator;
using containers::PooledNodeAllocator;

template <class Value, auto KeyBy, class NodeAllocatorType>
struct IsHashSet<HashSet<Value, KeyBy, NodeAllocatorType>> : std::true_type
{
};

} // namespace Hyperion
