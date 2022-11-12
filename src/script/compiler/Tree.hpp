#ifndef TREE_HPP
#define TREE_HPP

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
    TreeNode(const T &value)
        : m_value(value)
    {
    }

    ~TreeNode()
    {
        for (size_t i = 0; i < m_siblings.size(); i++) {
            if (m_siblings[i]) {
                delete m_siblings[i];
                m_siblings[i] = nullptr;
            }
        }
    }

    void PrintToStream(std::stringstream &ss, int &indent_level) const
    {
        for (int i = 0; i < indent_level; i++) {
            ss << "  ";
        }
        ss << m_value << "\n";

        indent_level++;
        for (int i = 0; i < m_siblings.size(); i++) {
            m_siblings[i]->PrintToStream(ss, indent_level);
        }
        indent_level--;
    }

    TreeNode<T> *m_parent = nullptr;
    std::vector<TreeNode<T>*> m_siblings;
    T m_value;
};

template <typename T>
class Tree
{
public:
    std::ostream &operator<<(std::ostream &os) const
    {
        std::stringstream ss;
        int indent_level = 0;

        for (int i = 0; i < m_nodes.size(); i++) {
            m_nodes[i]->PrintToStream(ss, indent_level);
        }

        os << ss.str() << "\n";

        return os;
    }

public:
    Tree() 
        : m_top(nullptr)
    {
        // open root
        Open(T());
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

        // first in, first out
        for (int i = m_nodes.size() - 1; i >= 0; i--) {
            if (m_nodes[i]) {
                delete m_nodes[i];
            }
        }
    }

    std::vector<TreeNode<T>*> &GetNodes() { return m_nodes; }
    const std::vector<TreeNode<T>*> &GetNodes() const { return m_nodes; }

    TreeNode<T> *TopNode() { return m_top; }
    const TreeNode<T> *TopNode() const { return m_top; }

    T &Top()
    {
        AssertThrow(m_top != nullptr);

        return m_top->m_value;
    }

    const T &Top() const
    {
        AssertThrow(m_top != nullptr);

        return m_top->m_value;
    }

    T &Root()
    {
        AssertThrow(!m_nodes.empty());

        return m_nodes.front()->m_value;
    }

    const T &Root() const
    {
        AssertThrow(!m_nodes.empty());

        return m_nodes.front()->m_value;
    }

    void Open(const T &value)
    {
        TreeNode<T> *node = new TreeNode<T>(value);
        node->m_parent = m_top;

        if (m_top) {
            m_top->m_siblings.push_back(node);
        } else {
            m_nodes.push_back(node);
        }

        m_top = node;
    }

    void Close()
    {
        AssertThrowMsg(m_top != nullptr, "Scope already closed");

        m_top = m_top->m_parent;
    }

private:
    std::vector<TreeNode<T>*> m_nodes;
    TreeNode<T> *m_top;
};

} // namespace hyperion::compielr

#endif
