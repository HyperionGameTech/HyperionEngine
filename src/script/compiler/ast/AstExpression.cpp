#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion {

AstExpression::AstExpression(
    const SourceLocation& location,
    int accessOptions)
    : AstStatement(location),
      m_accessOptions(accessOptions),
      m_accessMode(ACCESS_MODE_LOAD)
{
}

} // namespace hyperion
