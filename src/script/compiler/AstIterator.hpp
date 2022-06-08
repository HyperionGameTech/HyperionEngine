#ifndef AST_ITERATOR_HPP
#define AST_ITERATOR_HPP

#include <script/compiler/ast/AstStatement.hpp>

#include <system/debug.h>

#include <memory>
#include <vector>

namespace hyperion {
namespace compiler {

class AstIterator {
public:
    AstIterator();
    AstIterator(const AstIterator &other);

    inline void Push(const std::shared_ptr<AstStatement> &statement)
        { m_list.push_back(statement); }
    inline void Pop()
        { m_list.pop_back(); }

    inline int GetPosition() const
        { return m_position; }
    inline void ResetPosition()
        { m_position = 0; }
    inline void SetPosition(size_t position)
        { m_position = position; }

    inline std::shared_ptr<AstStatement> &Peek()
        { return m_list[m_position]; }
    inline const std::shared_ptr<AstStatement> &Peek() const
        { return m_list[m_position]; }
    inline std::shared_ptr<AstStatement> Next()
        { return m_list[m_position++]; }
    inline bool HasNext() const
        { return m_position < m_list.size(); }
        
    inline const SourceLocation &GetLocation() const
        { return m_list[m_position]->m_location; }

private:
    size_t m_position;
    std::vector<std::shared_ptr<AstStatement>> m_list;
};

} // namespace compiler
} // namespace hyperion

#endif
