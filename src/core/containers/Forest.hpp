#ifndef HYPERION_FOREST_HPP
#define HYPERION_FOREST_HPP

#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {
namespace containers {

/*! \brief Forest is a tree-like data structure that allows for multiple root nodes and supports parent-child relationships between nodes.
 *  It is useful for representing hierarchical data structures such as tree views.
 *  \tparam T The type of values stored in the forest nodes.
 */
template <class T>
class Forest
{
public:
    struct Iterator;

    class Node
    {
    public:
        friend Iterator;
        friend class Forest;

        Node(T&& value)
            : m_value(std::move(value)),
              m_parent(nullptr),
              m_prev(nullptr)
        {
        }

        HYP_FORCE_INLINE T& operator*()
        {
            return m_value;
        }

        HYP_FORCE_INLINE const T& operator*() const
        {
            return m_value;
        }

        HYP_FORCE_INLINE T* operator->()
        {
            return &m_value;
        }

        HYP_FORCE_INLINE const T* operator->() const
        {
            return &m_value;
        }

        bool HasChild(Node* node) const
        {
            Node* child = m_child.Get();

            while (child != nullptr)
            {
                if (child == node)
                {
                    return true;
                }

                child = child->m_next.Get();
            }

            return false;
        }

        bool HasChildDeep(Node* node) const
        {
            Node* child = m_child.Get();

            while (child != nullptr)
            {
                if (child == node || child->HasChildDeep(node))
                {
                    return true;
                }

                child = child->m_next.Get();
            }

            return false;
        }

        HYP_FORCE_INLINE Node* GetParent() const
        {
            return m_parent;
        }

        SizeType Size(int depth = -1) const
        {
            if (depth == 0)
            {
                return 0;
            }

            SizeType size = 0;

            Node* child = m_child.Get();

            while (child != nullptr)
            {
                size += 1 + child->Size(depth - 1);

                child = child->m_next.Get();
            }

            return size;
        }

    private:
        T m_value;
        UniquePtr<Node> m_next;
        UniquePtr<Node> m_child;
        Node* m_parent;
        Node* m_prev;
    };

    // @TODO: Change to not require a queue, instead,
    // advance inline
    struct Iterator
    {
        Iterator(Node* root)
        {
            // Add root and all siblings
            if (root)
            {
                Node* node = root;

                while (node != nullptr)
                {
                    queue.Push(node);

                    node = node->m_next.Get();
                }
            }

            Advance();
        }

        Iterator(const Iterator& other) = default;
        Iterator& operator=(const Iterator& other) = default;

        Iterator(Iterator&& other) noexcept = default;
        Iterator& operator=(Iterator&& other) noexcept = default;

        ~Iterator() = default;

        Iterator& operator++()
        {
            Advance();

            return *this;
        }

        Iterator operator++(int) const
        {
            Iterator result(*this);
            ++result;

            return result;
        }

        HYP_FORCE_INLINE T& operator*() const
        {
            return current_node->m_value;
        }

        HYP_FORCE_INLINE T* operator->() const
        {
            return &current_node->m_value;
        }

        HYP_FORCE_INLINE bool operator==(const Iterator& other) const
        {
            return current_node == other.current_node;
        }

        HYP_FORCE_INLINE bool operator!=(const Iterator& other) const
        {
            return current_node != other.current_node;
        }

        HYP_FORCE_INLINE Node* GetCurrentNode() const
        {
            return current_node;
        }

        void Advance()
        {
            if (queue.Any())
            {
                current_node = queue.Pop();

                if (current_node)
                {
                    Node* child = current_node->m_child.Get();

                    while (child != nullptr)
                    {
                        queue.Push(child);

                        child = child->m_next.Get();
                    }
                }
            }
            else
            {
                current_node = nullptr;
            }
        }

        Queue<Node*> queue;
        Node* current_node = nullptr;
    };

    using ConstIterator = Iterator;

    Forest()
    {
    }

    Iterator Add(T&& value, const T& parent)
    {
        auto parent_it = Find(parent);

        if (parent_it == End())
        {
            return Add(std::move(value), Iterator(nullptr));
        }

        return Add(std::move(value), parent_it);
    }

    Iterator Add(T&& value, ConstIterator parent_iterator = Iterator(nullptr))
    {
        UniquePtr<Node> new_node = MakeUnique<Node>(std::move(value));

        Node* ptr = new_node.Get();

        if (parent_iterator == End())
        {
            if (m_root)
            {
                Node* sibling = m_root.Get();

                while (sibling->m_next != nullptr)
                {
                    sibling = sibling->m_next.Get();
                }

                new_node->m_prev = sibling;
                sibling->m_next = std::move(new_node);
            }
            else
            {
                m_root = std::move(new_node);
            }

            return Iterator(ptr);
        }

        Node* parent = parent_iterator.current_node;

        ptr->m_parent = parent;

        if (parent->m_child != nullptr)
        {
            Node* sibling = parent->m_child.Get();

            while (sibling->m_next != nullptr)
            {
                sibling = sibling->m_next.Get();
            }

            new_node->m_prev = sibling;
            sibling->m_next = std::move(new_node);
        }
        else
        {
            parent->m_child = std::move(new_node);
        }

        return Iterator(ptr);
    }

    Iterator Erase(Iterator it)
    {
        if (it == End())
        {
            return End();
        }

        if (m_root.Get() == it.current_node)
        {
            // if we are the root, remove it
            m_root.Reset();

            return End();
        }

        Iterator next = it;
        next.Advance();

        if (it.current_node->m_next)
        {
            it.current_node->m_next->m_prev = it.current_node->m_prev;
        }

        if (it.current_node->m_prev)
        {
            UniquePtr<Node> _this_node = std::move(it.current_node->m_prev->m_next);
            // If we are a sibling in a chain, update the left sibling to point to our right sibling (if any)
            _this_node->m_prev->m_next = std::move(_this_node->m_next);
            // this is released
            return next;
        }

        if (it.current_node->m_parent && it.current_node->m_parent->m_child.Get() == it.current_node)
        {
            UniquePtr<Node> _this_node = std::move(it.current_node->m_parent->m_child);
            // If we are the first child, update the parent to refer to our right sibling (if any) as the first child
            _this_node->m_parent->m_child = std::move(_this_node->m_next);
            // this is released
            return next;
        }

        return next;
    }

    template <class FindAsType>
    Iterator Find(const FindAsType& value)
    {
        Queue<Node*> queue;

        { // Add root and all siblings
            Node* node = m_root.Get();

            while (node != nullptr)
            {
                queue.Push(node);

                node = node->m_next.Get();
            }
        }

        while (queue.Any())
        {
            Node* current_node = queue.Pop();

            if (!current_node)
            {
                continue;
            }

            if (current_node->m_value == value)
            {
                return Iterator(current_node);
            }

            Node* child = current_node->m_child.Get();

            while (child != nullptr)
            {
                queue.Push(child);

                child = child->m_next.Get();
            }
        }

        return End();
    }

    template <class FindAsType>
    HYP_FORCE_INLINE ConstIterator Find(const FindAsType& value) const
    {
        return const_cast<Forest*>(this)->Find(value);
    }

    template <class Predicate>
    Iterator FindIf(Predicate&& pred)
    {
        Queue<Node*> queue;

        { // Add root and all siblings
            Node* node = m_root.Get();

            while (node != nullptr)
            {
                queue.Push(node);

                node = node->m_next.Get();
            }
        }

        while (queue.Any())
        {
            Node* current_node = queue.Pop();

            if (!current_node)
            {
                continue;
            }

            if (pred(**current_node))
            {
                return Iterator(current_node);
            }

            Node* child = current_node->m_child.Get();

            while (child != nullptr)
            {
                queue.Push(child);

                child = child->m_next.Get();
            }
        }

        return End();
    }

    template <class Predicate>
    HYP_FORCE_INLINE ConstIterator FindIf(Predicate&& pred) const
    {
        return const_cast<Forest*>(this)->FindIf(std::forward<Predicate>(pred));
    }

    HYP_FORCE_INLINE void Clear()
    {
        m_root.Reset();
    }

    SizeType Size(int depth = -1) const
    {
        if (depth == 0)
        {
            return 0;
        }

        SizeType size = 0;

        Node* sibling = m_root.Get();

        while (sibling != nullptr)
        {
            size += 1 + sibling->Size(depth - 1);

            sibling = sibling->m_next.Get();
        }

        return size;
    }

    HYP_DEF_STL_BEGIN_END(Iterator(m_root.Get()), Iterator(nullptr))

private:
    UniquePtr<Node> m_root;
};

} // namespace containers

using containers::Forest;

} // namespace hyperion

#endif