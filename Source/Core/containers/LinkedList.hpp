/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/ContainerBase.hpp>

#include <Core/utilities/ValueStorage.hpp>

#include <Core/memory/Memory.hpp>

#include <Core/memory/allocator/Allocator.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/HashCode.hpp>
#include <Core/utilities/Traits.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

namespace containers {

template <class T, class AllocatorType>
struct LinkedListNode
{
    LinkedListNode* previous;
    LinkedListNode* next;
    ValueStorage<T> value;
};

template <class T, class AllocatorType = DynamicAllocator>
class LinkedList;

template <class T, class AllocatorType>
class LinkedList : public ContainerBase<LinkedList<T, AllocatorType>, size_t>
{
    using Node = containers::LinkedListNode<T, AllocatorType>;

public:
    static constexpr bool isContiguous = false;

    struct ConstIterator;

    struct Iterator
    {
        Node* node;

        HYP_FORCE_INLINE T& operator*()
        {
            return node->value.Get();
        }

        HYP_FORCE_INLINE const T& operator*() const
        {
            return node->value.Get();
        }

        HYP_FORCE_INLINE T* operator->()
        {
            return &node->value.Get();
        }

        HYP_FORCE_INLINE const T* operator->() const
        {
            return &node->value.Get();
        }

        HYP_FORCE_INLINE Iterator& operator++()
        {
            node = node->next;
            return *this;
        }

        HYP_FORCE_INLINE Iterator operator++(int)
        {
            return Iterator { node->next };
        }

        HYP_FORCE_INLINE Iterator operator+(size_t n) const
        {
            Iterator it = *this;
            it += n;
            return it;
        }

        Iterator& operator+=(size_t n)
        {
            for (size_t i = 0; i < n; i++)
            {
                HYP_CORE_ASSERT(node != nullptr);
                node = node->next;
            }

            return *this;
        }

        HYP_FORCE_INLINE Iterator& operator--()
        {
            node = node->previous;
            return *this;
        }

        HYP_FORCE_INLINE Iterator operator--(int)
        {
            return Iterator { node->previous };
        }

        HYP_FORCE_INLINE Iterator operator-(size_t n) const
        {
            Iterator it = *this;
            it -= n;
            return it;
        }

        Iterator& operator-=(size_t n)
        {
            for (size_t i = 0; i < n; i++)
            {
                HYP_CORE_ASSERT(node != nullptr);
                node = node->previous;
            }

            return *this;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return node == other.node;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return node != other.node;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return node == other.node;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return node != other.node;
        }
    };

    struct ConstIterator
    {
        const Node* node;

        HYP_FORCE_INLINE const T& operator*() const
        {
            return node->value.Get();
        }

        HYP_FORCE_INLINE const T* operator->() const
        {
            return &node->value.Get();
        }

        HYP_FORCE_INLINE ConstIterator& operator++()
        {
            node = node->next;
            return *this;
        }

        HYP_FORCE_INLINE ConstIterator operator++(int)
        {
            return ConstIterator { node->next };
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return node == other.node;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return node != other.node;
        }

        HYP_FORCE_INLINE bool operator==(const ConstIterator& other) const
        {
            return node == other.node;
        }

        HYP_FORCE_INLINE bool operator!=(const ConstIterator& other) const
        {
            return node != other.node;
        }
    };

    using Base = ContainerBase<LinkedList<T, AllocatorType>, size_t>;
    using KeyType = typename Base::KeyType;
    using ValueType = T;
    
    template <bool ConditionalEnable = HasDefaultAllocatorInstance<AllocatorType>, typename = std::enable_if_t<ConditionalEnable>>
    LinkedList()
        : m_pAllocator(GetDefaultAllocatorInstance<AllocatorType>()),
          m_head(nullptr),
          m_tail(nullptr),
          m_size(0)
    {
        HYP_CORE_ASSERT(m_pAllocator != nullptr);
    }

    LinkedList(const LinkedList& other);
    LinkedList(LinkedList&& other) noexcept;
    ~LinkedList();

    LinkedList& operator=(const LinkedList& other);
    LinkedList& operator=(LinkedList&& other) noexcept;

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE ValueType& Front()
    {
        return m_head->value.Get();
    }

    HYP_FORCE_INLINE const ValueType& Front() const
    {
        return m_head->value.Get();
    }

    HYP_FORCE_INLINE ValueType& Back()
    {
        return m_tail->value.Get();
    }

    HYP_FORCE_INLINE const ValueType& Back() const
    {
        return m_tail->value.Get();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Size() == 0;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return Size() != 0;
    }

    /*! \brief Access the element at \p index. Note that the LinkedList must be traversed until we reach the element,
     *  so this is operation is not O(1), but O(n) where n is the number of elements in the LinkedList */
    HYP_FORCE_INLINE T& operator[](size_t index)
    {
        HYP_CORE_ASSERT(index < m_size);

        Node* node = m_head;

        size_t curr = 0;

        while (curr < index)
        {
            HYP_CORE_ASSERT(node != nullptr);

            node = node->next;

            ++curr;
        }

        return node->value.Get();
    }

    /*! \brief Access the element at \p index. Note that the LinkedList must be traversed until we reach the element,
     *  so this is operation is not O(1), but O(n) where n is the number of elements in the LinkedList */
    HYP_FORCE_INLINE const T& operator[](size_t index) const
    {
        return const_cast<LinkedList<T, AllocatorType>*>(this)->operator[](index);
    }

    template <class... Args>
    ValueType& EmplaceBack(Args&&... args)
    {
        Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
        newNode->previous = m_tail;
        newNode->next = nullptr;
        newNode->value.Construct(std::forward<Args>(args)...);

        if (m_size == 0)
        {
            m_head = newNode;
            m_tail = m_head;
        }
        else
        {
            m_tail->next = newNode;
            m_tail = newNode;
        }

        ++m_size;

        return newNode->value.Get();
    }

    template <class... Args>
    ValueType& EmplaceFront(Args&&... args)
    {
        Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
        newNode->previous = nullptr;
        newNode->next = m_head;
        newNode->value.Construct(std::forward<Args>(args)...);

        if (m_size != 0)
        {
            m_head->previous = newNode;
            m_tail = m_head;
        }
        else
        {
            m_tail = newNode;
        }

        m_head = newNode;
        ++m_size;

        return newNode->value.Get();
    }

    /*! \brief Alias for PushBack(), but returns the inserted value directly. */
    HYP_FORCE_INLINE ValueType& Add(const ValueType& value)
    {
        return PushBack(value);
    }

    /*! \brief Alias for PushBack(), but returns the inserted value directly. */
    HYP_FORCE_INLINE ValueType& Add(ValueType&& value)
    {
        return PushBack(std::move(value));
    }

    /*! \brief Push an item to the back of the container.*/
    ValueType& PushBack(const ValueType& value);

    /*! \brief Push an item to the back of the container.*/
    ValueType& PushBack(ValueType&& value);

    /*! \brief Push an item to the front of the container.*/
    ValueType& PushFront(const ValueType& value);

    /*! \brief Push an item to the front of the container. */
    ValueType& PushFront(ValueType&& value);

    /*! \brief Pop an item from the back of the linked list. The popped item is returned. */
    ValueType PopBack();

    /*! \brief Pop an item from the front of the linked list. The popped item is returned. */
    ValueType PopFront();

    /*! \brief Erase an element by iterator.
     *  \returns An iterator to the next element after the erased element. */
    Iterator Erase(Iterator iter);

    /*! \brief Erase an element by const iterator.
     *  \returns An iterator to the next element after the erased element. */
    Iterator Erase(ConstIterator iter);

#if 0
    //! \brief Erase an element by value. A Find() is performed, and if the result is not equal to End(),
    //  the element is removed.
    Iterator Erase(const T &value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType &value);
    Iterator Insert(ConstIterator where, ValueType &&value);
#endif
    void Clear();

#if 0
    template <size_t OtherNumInlineBytes>
    [[nodiscard]]
    bool operator==(const Array<T, OtherNumInlineBytes> &other) const
    {
        if (this == &other) {
            return true;
        }

        if (Size() != other.Size()) {
            return false;
        }

        auto it = Begin();
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt) {
            if (!(*it == *otherIt)) {
                return false;
            }
        }

        return true;
    }

    template <size_t OtherNumInlineBytes>
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Array<T, OtherNumInlineBytes> &other) const
        { return !operator==(other); }

#endif

    HYP_DEF_STL_BEGIN_END({ m_head }, { (Node*)nullptr })

private:
    AllocatorType* const m_pAllocator;

    Node* m_head;
    Node* m_tail;
    size_t m_size;
};

template <class T, class AllocatorType>
LinkedList<T, AllocatorType>::LinkedList(const LinkedList<T, AllocatorType>& other)
    : m_pAllocator(other.m_pAllocator),
      m_head(nullptr),
      m_tail(nullptr),
      m_size(0)
{
    HYP_CORE_ASSERT(m_pAllocator != nullptr);

    for (const auto& value : other)
    {
        PushBack(value);
    }
}

template <class T, class AllocatorType>
LinkedList<T, AllocatorType>::LinkedList(LinkedList<T, AllocatorType>&& other) noexcept
    : m_pAllocator(other.m_pAllocator),
      m_head(other.m_head),
      m_tail(other.m_tail),
      m_size(other.m_size)
{
    HYP_CORE_ASSERT(m_pAllocator != nullptr);

    other.m_head = nullptr;
    other.m_tail = nullptr;
    other.m_size = 0;
}

template <class T, class AllocatorType>
LinkedList<T, AllocatorType>& LinkedList<T, AllocatorType>::operator=(const LinkedList<T, AllocatorType>& other)
{
    if (this == &other)
    {
        return *this;
    }

    Clear();

    for (const auto& value : other)
    {
        PushBack(value);
    }

    return *this;
}

template <class T, class AllocatorType>
LinkedList<T, AllocatorType>& LinkedList<T, AllocatorType>::operator=(LinkedList<T, AllocatorType>&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Clear();

    m_head = other.m_head;
    m_tail = other.m_tail;
    m_size = other.m_size;

    other.m_head = nullptr;
    other.m_tail = nullptr;
    other.m_size = 0;

    return *this;
}

template <class T, class AllocatorType>
LinkedList<T, AllocatorType>::~LinkedList()
{
    Node* node = m_head;

    while (node != nullptr)
    {
        Node* next = node->next;

        node->value.Destruct();
        m_pAllocator->Free(node);

        node = next;
    }
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PushBack(const ValueType& value) -> ValueType&
{
    Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
    newNode->previous = m_tail;
    newNode->next = nullptr;
    newNode->value.Construct(value);

    if (m_size == 0)
    {
        m_head = newNode;
        m_tail = m_head;
    }
    else
    {
        m_tail->next = newNode;
        m_tail = newNode;
    }

    ++m_size;

    return newNode->value.Get();
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PushBack(ValueType&& value) -> ValueType&
{
    Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
    newNode->previous = m_tail;
    newNode->next = nullptr;
    newNode->value.Construct(std::move(value));

    if (m_size == 0)
    {
        m_head = newNode;
        m_tail = m_head;
    }
    else
    {
        m_tail->next = newNode;
        m_tail = newNode;
    }

    ++m_size;

    return newNode->value.Get();
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PushFront(const ValueType& value) -> ValueType&
{
    Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
    newNode->previous = nullptr;
    newNode->next = m_head;
    newNode->value.Construct(value);

    if (m_size != 0)
    {
        m_head->previous = newNode;
        m_tail = m_head;
    }
    else
    {
        m_tail = newNode;
    }

    m_head = newNode;

    ++m_size;

    return newNode->value.Get();
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PushFront(ValueType&& value) -> ValueType&
{
    Node* newNode = (Node*)m_pAllocator->Allocate(sizeof(Node), alignof(Node));
    newNode->previous = nullptr;
    newNode->next = m_head;
    newNode->value.Construct(std::move(value));

    if (m_size != 0)
    {
        m_head->previous = newNode;
        m_tail = m_head;
    }
    else
    {
        m_tail = newNode;
    }

    m_head = newNode;

    ++m_size;

    return newNode->value.Get();
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PopBack() -> ValueType
{
    HYP_CORE_ASSERT(m_size != 0);

    Node* prev = m_tail->previous;

    ValueType value = std::move(m_tail->value.Get());

    m_tail->value.Destruct();
    m_pAllocator->Free(m_tail);

    if (prev)
    {
        prev->next = nullptr;
    }

    m_tail = prev;

    if (!--m_size)
    {
        m_head = nullptr;
    }

    return value;
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::PopFront() -> ValueType
{
    HYP_CORE_ASSERT(m_size != 0);

    Node* next = m_head->next;

    ValueType value = std::move(m_head->value.Get());

    m_head->value.Destruct();
    m_pAllocator->Free(m_head);

    if (next)
    {
        next->previous = nullptr;
    }

    m_head = next;

    if (!--m_size)
    {
        m_tail = nullptr;
    }

    return value;
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::Erase(Iterator iter) -> Iterator
{
    if (iter == End())
    {
        return End();
    }

    Node* node = iter.node;
    Node* prev = node->previous;

    if (prev)
    {
        prev->next = node->next;
    }

    Node* next = node->next;

    if (next)
    {
        next->previous = prev;
    }

    if (node == m_head)
    {
        m_head = next;
    }

    if (node == m_tail)
    {
        m_tail = prev;
    }

    node->value.Destruct();
    m_pAllocator->Free(node);

    --m_size;

    return Iterator { next };
}

template <class T, class AllocatorType>
auto LinkedList<T, AllocatorType>::Erase(ConstIterator iter) -> Iterator
{
    if (iter == End())
    {
        return End();
    }

    Node* node = iter.node;
    Node* prev = node->previous;

    if (prev)
    {
        prev->next = node->next;
    }

    Node* next = node->next;

    if (next)
    {
        next->previous = prev;
    }

    if (node == m_head)
    {
        m_head = next;
    }

    if (node == m_tail)
    {
        m_tail = prev;
    }

    node->value.Destruct();
    m_pAllocator->Free(node);

    --m_size;

    return Iterator { next };
}

template <class T, class AllocatorType>
void LinkedList<T, AllocatorType>::Clear()
{
    Node* node = m_head;

    while (node != nullptr)
    {
        Node* next = node->next;

        node->value.Destruct();
        m_pAllocator->Free(node);

        node = next;
    }

    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
}

} // namespace containers

using containers::LinkedList;

template <class T, class AllocatorType>
struct IsLinkedList<containers::LinkedList<T, AllocatorType>> : std::true_type
{
};

} // namespace Hyperion
