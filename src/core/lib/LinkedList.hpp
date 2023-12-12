#ifndef HYPERION_V2_LIB_LINKED_LIST_HPP
#define HYPERION_V2_LIB_LINKED_LIST_HPP

#include "ContainerBase.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>
#include <core/lib/CMemory.hpp>
#include <system/Debug.hpp>
#include <HashCode.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

namespace containers {
namespace detail {

template <class T>
struct LinkedListNode
{
    LinkedListNode *previous = nullptr;
    LinkedListNode *next = nullptr;
    ValueStorage<T> value;
};

} // namespace detail
} // namespace containers

template <class T>
class LinkedList : public ContainerBase<LinkedList<T>, SizeType>
{
    using Node = containers::detail::LinkedListNode<T>;

public:
    static constexpr Bool is_contiguous = false;
    
    struct ConstIterator;

    struct Iterator
    {
        Node *node;

        T &operator*()
        {
            AssertThrow(node != nullptr);
            return node->value.Get();
        }

        const T &operator*() const
        {
            AssertThrow(node != nullptr);
            return node->value.Get();
        }

        T *operator->()
        {
            AssertThrow(node != nullptr);
            return &node->value.Get();
        }

        const T *operator->() const
        {
            AssertThrow(node != nullptr);
            return &node->value.Get();
        }

        Iterator &operator++()
        {
            AssertThrow(node != nullptr);
            node = node->next;
            return *this;
        }

        Iterator operator++(int)
        {
            AssertThrow(node != nullptr);
            return Iterator { node->next };
        }

        bool operator==(const Iterator &other) const
            { return node == other.node; }

        bool operator!=(const Iterator &other) const
            { return node != other.node; }

        bool operator==(const ConstIterator &other) const
            { return node == other.node; }

        bool operator!=(const ConstIterator &other) const
            { return node != other.node; }
    };

    struct ConstIterator
    {
        const Node *node;
        
        const T &operator*() const
        {
            AssertThrow(node != nullptr);
            return node->value.Get();
        }

        const T *operator->() const
        {
            AssertThrow(node != nullptr);
            return &node->value.Get();
        }
        
        ConstIterator &operator++()
        {
            AssertThrow(node != nullptr);
            node = node->next;
            return *this;
        }

        ConstIterator operator++(int)
        {
            AssertThrow(node != nullptr);
            return ConstIterator { node->next };
        }

        bool operator==(const Iterator &other) const
            { return node == other.node; }

        bool operator!=(const Iterator &other) const
            { return node != other.node; }

        bool operator==(const ConstIterator &other) const
            { return node == other.node; }

        bool operator!=(const ConstIterator &other) const
            { return node != other.node; }
    };

    using Base = ContainerBase<LinkedList<T>, SizeType>;
    using KeyType = typename Base::KeyType;
    using ValueType = T;

    LinkedList();
    //LinkedList(const LinkedList &other);
    //LinkedList(LinkedList &&other) noexcept;
    ~LinkedList();

    //LinkedList &operator=(const LinkedList &other);
    //LinkedList &operator=(LinkedList &&other) noexcept;

    [[nodiscard]] SizeType Size() const
        { return m_size; }

    [[nodiscard]] ValueType &Front()
        { AssertThrow(m_size != 0); return m_head->value.Get(); }

    [[nodiscard]] const ValueType &Front() const
        { AssertThrow(m_size != 0); return m_head->value.Get(); }

    [[nodiscard]] ValueType &Back()
        { AssertThrow(m_size != 0); return m_tail->value.Get(); }

    [[nodiscard]] const ValueType &Back() const
        { AssertThrow(m_size != 0); return m_tail->value.Get(); }

    [[nodiscard]] bool Empty() const
        { return Size() == 0; }

    [[nodiscard]] bool Any() const
        { return Size() != 0; }
    
    template <class ...Args>
    void EmplaceBack(Args &&... args)
    {
        Node *new_node = new Node;
        new_node->previous = m_tail;
        new_node->value.Construct(std::forward<Args>(args)...);
        
        if (m_size == 0) {
            m_head = new_node;
            m_tail = m_head;
        } else {
            m_tail->next = new_node;
            m_tail = new_node;
        }

        ++m_size;
    }

    template <class ...Args>
    void EmplaceFront(Args &&... args)
    {
        Node *new_node = new Node;
        new_node->next = m_head;
        new_node->value.Construct(std::forward<Args>(args)...);

        if (m_size != 0) {
            m_head->previous = new_node;
            m_tail = m_head;
        } else {
            m_tail = new_node;
        }

        m_head = new_node;
        ++m_size;
    }
    
    void PushBack(const ValueType &value);
    void PushBack(ValueType &&value);

    /*! Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time.
        */
    void PushFront(const ValueType &value);

    /*! Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time. */
    void PushFront(ValueType &&value);

    ValueType PopBack();

#if 0
    //! \brief Erase an element by iterator.
    Iterator Erase(ConstIterator iter);
    //! \brief Erase an element by value. A Find() is performed, and if the result is not equal to End(),
    //  the element is removed.
    Iterator Erase(const T &value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType &value);
    Iterator Insert(ConstIterator where, ValueType &&value);
    ValueType PopFront();
    ValueType PopBack();
    void Clear();
#endif

#if 0
    template <SizeType OtherNumInlineBytes>
    [[nodiscard]] bool operator==(const DynArray<T, OtherNumInlineBytes> &other) const
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
    [[nodiscard]] bool operator!=(const DynArray<T, OtherNumInlineBytes> &other) const
        { return !operator==(other); }

#endif
    
    HYP_DEF_STL_BEGIN_END(
        { m_head },
        { (Node *)nullptr }
    )

private:
    Node *m_head;
    Node *m_tail;
    SizeType m_size;
};

template <class T>
LinkedList<T>::LinkedList()
    : m_head(nullptr),
      m_tail(nullptr),
      m_size(0)
{
    //m_head->previous = m_head;
}

template <class T>
LinkedList<T>::~LinkedList()
{
    Node *node = m_head;

    while (node != nullptr) {
        Node *next = node->next;

        node->value.Destruct();
        delete node;

        node = next;
    }
}

template <class T>
void LinkedList<T>::PushBack(const ValueType &value)
{
    Node *new_node = new Node;
    new_node->previous = m_tail;
    new_node->value.Construct(value);
    
    if (m_size == 0) {
        m_head = new_node;
        m_tail = m_head;
    } else {
        m_tail->next = new_node;
        m_tail = new_node;
    }

    ++m_size;
}

template <class T>
void LinkedList<T>::PushBack(ValueType &&value)
{
    Node *new_node = new Node;
    new_node->previous = m_tail;
    new_node->value.Construct(std::move(value));

    if (m_size == 0) {
        m_head = new_node;
        m_tail = m_head;
    } else {
        m_tail->next = new_node;
        m_tail = new_node;
    }

    ++m_size;
}

template <class T>
void LinkedList<T>::PushFront(const ValueType &value)
{
    Node *new_node = new Node;
    new_node->next = m_head;
    new_node->value.Construct(value);

    if (m_size != 0) {
        m_head->previous = new_node;
        m_tail = m_head;
    } else {
        m_tail = new_node;
    }

    m_head = new_node;

    ++m_size;
}

template <class T>
void LinkedList<T>::PushFront(ValueType &&value)
{
    Node *new_node = new Node;
    new_node->next = m_head;
    new_node->value.Construct(std::move(value));

    if (m_size != 0) {
        m_head->previous = new_node;
        m_tail = m_head;
    } else {
        m_tail = new_node;
    }

    m_head = new_node;

    ++m_size;
}

template <class T>
auto LinkedList<T>::PopBack() -> ValueType
{
    AssertThrow(m_size != 0);

    Node *prev = m_tail->previous;

    ValueType value = std::move(m_tail->value.Get());

    m_tail->value.Destruct();
    delete m_tail;

    if (prev) {
        prev->next = nullptr;
    }

    m_tail = prev;

    if (!--m_size) {
        m_head = nullptr;
    }

    return value;
}

} // namespace hyperion

#endif