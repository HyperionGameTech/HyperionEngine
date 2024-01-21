#include <script/compiler/ast/AstStatement.hpp>

namespace hyperion::compiler {

const String AstStatement::unnamed = "<unnamed>";

AstStatement::AstStatement(const SourceLocation &location)
    : m_location(location),
      m_scope_depth(0)
{
}

} // namespace hyperion::compiler
