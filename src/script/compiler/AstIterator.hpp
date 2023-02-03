#ifndef AST_ITERATOR_HPP
#define AST_ITERATOR_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstIterator
{
public:
    AstIterator();
    AstIterator(const AstIterator &other);

    void Push(const RC<AstStatement> &statement) { m_list.push_back(statement); }
    void Pop() { m_list.pop_back(); }

    SizeType GetPosition() const { return m_position; }
    void ResetPosition() { m_position = 0; }
    void SetPosition(size_t position) { m_position = position; }

    SizeType GetSize() const { return m_list.size(); }

    RC<AstStatement> &Peek() { return m_list[m_position]; }
    const RC<AstStatement> &Peek() const { return m_list[m_position]; }
    RC<AstStatement> Next()  { return m_list[m_position++]; }
    bool HasNext() const { return m_position < m_list.size(); }
        
    const SourceLocation &GetLocation() const { return m_list[m_position]->m_location; }

private:
    SizeType m_position;
    std::vector<RC<AstStatement>> m_list;
};

} // namespace hyperion::compiler

#endif
