#include <script/compiler/ast/AstStatement.hpp>

namespace hyperion {

const String AstStatement::unnamed = "<unnamed>";

AstStatement::AstStatement(const SourceLocation& location)
    : m_location(location),
      m_scopeDepth(0)
{
}

} // namespace hyperion
