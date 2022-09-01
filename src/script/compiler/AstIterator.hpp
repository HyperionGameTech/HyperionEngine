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

    void Push(const std::shared_ptr<AstStatement> &statement) { m_list.push_back(statement); }
    void Pop() { m_list.pop_back(); }

    int GetPosition() const { return m_position; }
    void ResetPosition() { m_position = 0; }
    void SetPosition(size_t position) { m_position = position; }

    SizeType GetSize() const { return m_list.size(); }

    std::shared_ptr<AstStatement> &Peek() { return m_list[m_position]; }
    const std::shared_ptr<AstStatement> &Peek() const { return m_list[m_position]; }
    std::shared_ptr<AstStatement> Next()  { return m_list[m_position++]; }
    bool HasNext() const { return m_position < m_list.size(); }
        
    const SourceLocation &GetLocation() const { return m_list[m_position]->m_location; }

private:
    SizeType m_position;
    std::vector<std::shared_ptr<AstStatement>> m_list;
};

} // namespace hyperion::compiler

#endif
