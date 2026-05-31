#include <Lang/compiler/ast/AstStatement.hpp>

namespace Hyperion {

const String AstStatement::s_unnamed = "<unnamed>";

AstStatement::AstStatement(const SourceLocation& location)
    : m_location(location),
      m_scopeDepth(0)
{
}

} // namespace Hyperion
