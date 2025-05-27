/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LINKED_LIST_HPP
#define HYPERION_LINKED_LIST_HPP

#include <core/containers/ContainerBase.hpp>
#include <core/utilities/ValueStorage.hpp>
#include <core/Defines.hpp>
#include <core/memory/Memory.hpp>
#include <core/debug/Debug.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

namespace containers {
namespace detail {

template <class T>
struct LinkedListNode
{
    LinkedListNode* previous = nullptr;
    LinkedListNode* next = nullptr;
    ValueStorage<T> value;
};

} // namespace detail

template <class T>
class LinkedList : public ContainerBase<LinkedList<T>, SizeType>
{
    using Node = containers::detail::LinkedListNode<T>;

public:
    static constexpr bool is_contiguous = false;

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

    using Base = ContainerBase<LinkedList<T>, SizeType>;
    using KeyType = typename Base::KeyType;
    using ValueType = T;

    LinkedList();
    LinkedList(const LinkedList& other);
    LinkedList(LinkedList&& other) noexcept;
    ~LinkedList();

    LinkedList& operator=(const LinkedList& other);
    LinkedList& operator=(LinkedList&& other) noexcept;

    HYP_FORCE_INLINE SizeType Size() const
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

    /*! \brief Access the element at \ref{index}. Note that the LinkedList must be traversed until we reach the element,
     *  so this is operation is not O(1), but O(n) where n is the number of elements in the LinkedList */
    HYP_FORCE_INLINE T& operator[](SizeType index)
    {
        AssertThrow(index < m_size);

        Node* node = m_head;

        SizeType curr = 0;

        while (curr < index)
        {
            AssertThrow(node != nullptr);

            node = node->next;

            ++curr;
        }

        return node->value.Get();
    }

    /*! \brief Access the element at \ref{index}. Note that the LinkedList must be traversed until we reach the element,
     *  so this is operation is not O(1), but O(n) where n is the number of elements in the LinkedList */
    HYP_FORCE_INLINE const T& operator[](SizeType index) const
    {
        return const_cast<LinkedList<T>*>(this)->operator[](index);
    }

    template <class... Args>
    ValueType& EmplaceBack(Args&&... args)
    {
        Node* new_node = new Node;
        new_node->previous = m_tail;
        new_node->value.Construct(std::forward<Args>(args)...);

        if (m_size == 0)
        {
            m_head = new_node;
            m_tail = m_head;
        }
        else
        {
            m_tail->next = new_node;
            m_tail = new_node;
        }

        ++m_size;

        return new_node->value.Get();
    }

    template <class... Args>
    ValueType& EmplaceFront(Args&&... args)
    {
        Node* new_node = new Node;
        new_node->next = m_head;
        new_node->value.Construct(std::forward<Args>(args)...);

        if (m_size != 0)
        {
            m_head->previous = new_node;
            m_tail = m_head;
        }
        else
        {
            m_tail = new_node;
        }

        m_head = new_node;
        ++m_size;

        return new_node->value.Get();
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
    template <SizeType OtherNumInlineBytes>
    [[nodiscard]]
    bool operator==(const Array<T, OtherNumInlineBytes> &other) const
    {
        if (std::addressof(other) == this) {
            return true;
        }

        if (Size() != other.Size()) {
            return false;
        }

        auto it = Begin();
        auto other_it = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++other_it) {
            if (!(*it == *other_it)) {
                return false;
            }
        }

        return true;
    }

    template <SizeType OtherNumInlineBytes>
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Array<T, OtherNumInlineBytes> &other) const
        { return !operator==(other); }

#endif

    HYP_DEF_STL_BEGIN_END(
        { m_head },
        { (Node*)nullptr })

private:
    Node* m_head;
    Node* m_tail;
    SizeType m_size;
};

template <class T>
LinkedList<T>::LinkedList()
    : m_head(nullptr),
      m_tail(nullptr),
      m_size(0)
{
}

template <class T>
LinkedList<T>::LinkedList(const LinkedList<T>& other)
    : m_head(nullptr),
      m_tail(nullptr),
      m_size(0)
{
    for (const auto& value : other)
    {
        PushBack(value);
    }
}

template <class T>
LinkedList<T>::LinkedList(LinkedList<T>&& other) noexcept
    : m_head(other.m_head),
      m_tail(other.m_tail),
      m_size(other.m_size)
{
    other.m_head = nullptr;
    other.m_tail = nullptr;
    other.m_size = 0;
}

template <class T>
LinkedList<T>& LinkedList<T>::operator=(const LinkedList<T>& other)
{
    if (std::addressof(other) == this)
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

template <class T>
LinkedList<T>& LinkedList<T>::operator=(LinkedList<T>&& other) noexcept
{
    if (std::addressof(other) == this)
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

template <class T>
LinkedList<T>::~LinkedList()
{
    Node* node = m_head;

    while (node != nullptr)
    {
        Node* next = node->next;

        node->value.Destruct();
        delete node;

        node = next;
    }
}

template <class T>
auto LinkedList<T>::PushBack(const ValueType& value) -> ValueType&
{
    Node* new_node = new Node;
    new_node->previous = m_tail;
    new_node->value.Construct(value);

    if (m_size == 0)
    {
        m_head = new_node;
        m_tail = m_head;
    }
    else
    {
        m_tail->next = new_node;
        m_tail = new_node;
    }

    ++m_size;

    return new_node->value.Get();
}

template <class T>
auto LinkedList<T>::PushBack(ValueType&& value) -> ValueType&
{
    Node* new_node = new Node;
    new_node->previous = m_tail;
    new_node->value.Construct(std::move(value));

    if (m_size == 0)
    {
        m_head = new_node;
        m_tail = m_head;
    }
    else
    {
        m_tail->next = new_node;
        m_tail = new_node;
    }

    ++m_size;

    return new_node->value.Get();
}

template <class T>
auto LinkedList<T>::PushFront(const ValueType& value) -> ValueType&
{
    Node* new_node = new Node;
    new_node->next = m_head;
    new_node->value.Construct(value);

    if (m_size != 0)
    {
        m_head->previous = new_node;
        m_tail = m_head;
    }
    else
    {
        m_tail = new_node;
    }

    m_head = new_node;

    ++m_size;

    return new_node->value.Get();
}

template <class T>
auto LinkedList<T>::PushFront(ValueType&& value) -> ValueType&
{
    Node* new_node = new Node;
    new_node->next = m_head;
    new_node->value.Construct(std::move(value));

    if (m_size != 0)
    {
        m_head->previous = new_node;
        m_tail = m_head;
    }
    else
    {
        m_tail = new_node;
    }

    m_head = new_node;

    ++m_size;

    return new_node->value.Get();
}

template <class T>
auto LinkedList<T>::PopBack() -> ValueType
{
    AssertThrow(m_size != 0);

    Node* prev = m_tail->previous;

    ValueType value = std::move(m_tail->value.Get());

    m_tail->value.Destruct();
    delete m_tail;

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

template <class T>
auto LinkedList<T>::PopFront() -> ValueType
{
    AssertThrow(m_size != 0);

    Node* next = m_head->next;

    ValueType value = std::move(m_head->value.Get());

    m_head->value.Destruct();
    delete m_head;

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

template <class T>
auto LinkedList<T>::Erase(Iterator iter) -> Iterator
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
    delete node;

    --m_size;

    return Iterator { next };
}

template <class T>
void LinkedList<T>::Clear()
{
    Node* node = m_head;

    while (node != nullptr)
    {
        Node* next = node->next;

        node->value.Destruct();
        delete node;

        node = next;
    }

    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
}
} // namespace containers

template <class T>
using LinkedList = containers::LinkedList<T>;

} // namespace hyperion

#endif