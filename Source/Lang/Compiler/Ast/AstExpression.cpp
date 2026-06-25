#include <Lang/Compiler/Ast/AstExpression.hpp>

#include <AstExpression.generated.inl>

namespace Hyperion {

AstExpression::AstExpression(
    const SourceLocation& location,
    int accessOptions)
    : AstStatement(location),
      m_accessOptions(accessOptions),
      m_accessMode(ACCESS_MODE_LOAD)
{
}

} // namespace Hyperion
