#ifndef TREE_HPP
#define TREE_HPP

#include <core/lib/ValueStorage.hpp>

#include <system/Debug.hpp>

#include <vector>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <stack>
#include <string>

namespace hyperion::compiler {

template <typename T>
struct TreeNode
{
    template <class ... Args>
    TreeNode(Args &&... args)
    {
        m_value.Construct(std::forward<Args>(args)...);
    }

    TreeNode(const TreeNode &other) = delete;
    TreeNode &operator=(const TreeNode &other) = delete;
    TreeNode(TreeNode &&other) noexcept = delete;
    TreeNode &operator=(TreeNode &&other) noexcept = delete;

    ~TreeNode()
    {
        m_value.Destruct();

        for (SizeType i = 0; i < m_siblings.Size(); i++) {
            if (m_siblings[i]) {
                delete m_siblings[i];
                m_siblings[i] = nullptr;
            }
        }
    }

    void PrintToStream(std::stringstream &ss, int &indent_level) const
    {
        for (Int i = 0; i < indent_level; i++) {
            ss << "  ";
        }
        ss << m_value.Get() << "\n";

        indent_level++;
        for (Int i = 0; i < m_siblings.Size(); i++) {
            m_siblings[i]->PrintToStream(ss, indent_level);
        }

        indent_level--;
    }

    T &Get()
        { return m_value.Get(); }

    const T &Get() const
        { return m_value.Get(); }

    TreeNode<T>         *m_parent = nullptr;
    Array<TreeNode<T>*> m_siblings;
    ValueStorage<T>     m_value;
    UInt                m_depth = 0;
};

template <typename T>
class Tree
{
public:
    std::ostream &operator<<(std::ostream &os) const
    {
        std::stringstream ss;
        Int indent_level = 0;

        for (int i = 0; i < m_nodes.Size(); i++) {
            m_nodes[i]->PrintToStream(ss, indent_level);
        }

        os << ss.str() << "\n";

        return os;
    }

public:
    template <class ... Args>
    Tree(Args && ...args) 
        : m_top(nullptr)
    {
        // open root
        Open(std::forward<Args>(args)...);
    }

    Tree(const T &root) 
        : m_top(nullptr)
    {
        // open root
        Open(root);
    }

    ~Tree()
    {
        // close root
        Close();

        // LIFO
        for (SizeType index = m_nodes.Size(); index != 0; --index) {
            delete m_nodes[index - 1];
        }
    }

    Array<TreeNode<T>*> &GetNodes()
        { return m_nodes; }

    const Array<TreeNode<T>*> &GetNodes() const
        { return m_nodes; }

    TreeNode<T> *TopNode() { return m_top; }
    const TreeNode<T> *TopNode() const { return m_top; }

    T &Top()
    {
        AssertThrow(m_top != nullptr);

        return m_top->m_value.Get();
    }

    const T &Top() const
    {
        AssertThrow(m_top != nullptr);

        return m_top->m_value.Get();
    }

    T &Root()
    {
        AssertThrow(!m_nodes.Empty());

        return m_nodes.Front()->m_value.Get();
    }

    const T &Root() const
    {
        AssertThrow(!m_nodes.Empty());

        return m_nodes.Front()->m_value.Get();
    }

    template <class ...Args>
    void Open(Args &&... args)
    {
        TreeNode<T> *node = new TreeNode<T>(std::forward<Args>(args)...);
        node->m_parent = m_top;
        node->m_depth = m_top ? m_top->m_depth + 1 : 0;

        if (m_top) {
            m_top->m_siblings.PushBack(node);
        } else {
            m_nodes.PushBack(node);
        }

        m_top = node;
    }

    void Close()
    {
        AssertThrowMsg(m_top != nullptr, "Scope already closed");

        m_top = m_top->m_parent;
    }

    template <class Lambda>
    T *FindClosestMatch(Lambda lambda) const
    {
        TreeNode<T> *top = m_top;

        while (top) {
            if (lambda(top, top->m_value.Get())) {
                return &top->m_value.Get();
            }

            top = top->m_parent;
        }

        return nullptr;
    }

private:
    Array<TreeNode<T>*> m_nodes;
    TreeNode<T>         *m_top;
};

template <class T>
struct TreeNodeGuard
{
    Tree<T>     *m_tree;
    TreeNode<T> *m_node;

    template <class ... Args>
    TreeNodeGuard(Tree<T> *tree, Args &&... args)
        : m_tree(tree),
          m_node(nullptr)
    {
        m_tree->Open(std::forward<Args>(args)...);
        m_node = m_tree->TopNode();
    }

    TreeNodeGuard(const TreeNodeGuard &other) = delete;
    TreeNodeGuard &operator=(const TreeNodeGuard &other) = delete;
    TreeNodeGuard(TreeNodeGuard &&other) noexcept = delete;
    TreeNodeGuard &operator=(TreeNodeGuard &&other) noexcept = delete;

    ~TreeNodeGuard()
    {
        m_tree->Close();
    }

    T *operator->()
        { return &m_node->m_value.Get(); }

    const T *operator->() const
        { return &m_node->m_value.Get(); }
};

} // namespace hyperion::compielr

#endif
