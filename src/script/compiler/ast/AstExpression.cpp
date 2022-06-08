#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion {
namespace compiler {

AstExpression::AstExpression(
  const SourceLocation &location,
  int access_options)
    : AstStatement(location),
      m_access_options(access_options),
      m_access_mode(ACCESS_MODE_LOAD),
      m_is_standalone(false)
{
}

} // namespace compiler
} // namespace hyperion
